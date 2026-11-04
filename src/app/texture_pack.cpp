#include "app/texture_pack.hpp"

#include <format>
#include <optional>
#include <string>
#include <utility>

#include "app/hq_textures.hpp"
#include "app/image_dump.hpp"
#include "app/texture_keys.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "mods/mod_manifest.hpp"
#include "settings/toml_file.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kManifestName = "mod.toml";
constexpr std::string_view kFilesFolder = "files";

std::string FileNameOf(const std::string& gamePath) {
    const std::size_t slash = gamePath.rfind('/');
    return slash == std::string::npos ? gamePath : gamePath.substr(slash + 1);
}

Result<std::optional<assets::ImageAsset>> PackImage(const assets::Vfs& vfs, std::size_t gameLayer,
                                                    const ColorKeys& keys,
                                                    const std::string& gamePath) {
    const std::optional<assets::TextureFileKind> kind = ImageKindOf(gamePath);
    if (!kind) {
        return std::nullopt;
    }
    Result<std::optional<assets::FileData>> data = vfs.ReadLayer(gameLayer, gamePath);
    if (!data) {
        return std::unexpected(std::move(data.error()));
    }
    if (!*data) {
        return std::nullopt;
    }
    Result<assets::ImageAsset> image = assets::DecodeImage((*data)->Bytes(), *kind);
    const auto key = keys.byName.find(FileNameOf(gamePath));
    if (image && key != keys.byName.end()) {
        assets::ApplyColorKey(*image, key->second);
        if (!image->enhancedRgba.empty()) {
            image->rgba = std::move(image->enhancedRgba);
            image->enhancedRgba.clear();
        }
    }
    return image.transform(
        [](assets::ImageAsset found) { return std::optional(std::move(found)); });
}

Result<void> WriteManifest(const std::filesystem::path& path, const std::string& id) {
    const settings::TomlFile manifest{
        .table =
            toml::table{
                {"id", id}, {"name", "Texture pack"}, {"version", "0.1.0"}, {"enabled", false}},
        .leadingComments = {}};
    return settings::WriteTomlFile(manifest, path);
}

}

Result<void> DumpTexturePack(const assets::Vfs& vfs, std::size_t gameLayer,
                             const std::filesystem::path& modFolder) {
    const std::string id = Utf8FromPath(modFolder.filename());
    if (!mods::IsValidModId(id)) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} isn't a mod id: use only a-z, 0-9, _ and -", id));
    }
    const std::filesystem::path manifest = modFolder / kManifestName;
    std::error_code error;
    if (std::filesystem::exists(manifest, error) || error) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} already exists, so the pack isn't overwritten",
                                Utf8FromPath(manifest)));
    }
    Result<ColorKeys> keys = CollectColorKeys(vfs, gameLayer);
    if (!keys) {
        return std::unexpected(std::move(keys.error()));
    }
    for (const std::string& name : keys->conflicts) {
        log::Warn("{} has more than one colour key, using the first", name);
    }
    const std::filesystem::path files = modFolder / kFilesFolder;
    const ImageDump dump{
        .decode =
            [&](const std::string& gamePath) { return PackImage(vfs, gameLayer, *keys, gamePath); },
        .target =
            [&](const std::string& gamePath) {
                return files / PathFromUtf8(PngSibling(gamePath).value_or(gamePath));
            }};
    return DumpEachImage(vfs, dump).and_then([&](const DumpCounts& counts) {
        log::Info("dumped {} textures to {}, {} didn't decode", counts.saved, Utf8FromPath(files),
                  counts.failed);
        return WriteManifest(manifest, id);
    });
}

}
