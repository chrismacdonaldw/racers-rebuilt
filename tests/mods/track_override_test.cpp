#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "assets/jam_archive.hpp"
#include "assets/loader_pool.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "game_files.hpp"
#include "mods/mod_list.hpp"
#include "settings/registry.hpp"
#include "temp_folder.hpp"

namespace rr::mods {
namespace {

constexpr std::string_view kTrack = "GAMEDATA/RACEC0R0";
constexpr std::uint32_t kSidePx = 8;
constexpr std::string_view kManifest = R"(id = "paint"
name = "Paint"
version = "1.0"
)";
constexpr std::array<std::uint8_t, 3> kColor{200, 40, 160};

void AppendLittleEndian(std::vector<char>& bytes, std::uint32_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        bytes.push_back(static_cast<char>((value >> (8U * i)) & 0xFFU));
    }
}

void WriteSolidBmp(const std::filesystem::path& path) {
    constexpr std::uint32_t kHeaderBytes = 54;
    constexpr std::uint32_t kPixelBytes = kSidePx * kSidePx * 3;
    std::vector<char> bytes{'B', 'M'};
    AppendLittleEndian(bytes, kHeaderBytes + kPixelBytes, 4);
    AppendLittleEndian(bytes, 0, 4);
    AppendLittleEndian(bytes, kHeaderBytes, 4);
    AppendLittleEndian(bytes, 40, 4);
    AppendLittleEndian(bytes, kSidePx, 4);
    AppendLittleEndian(bytes, kSidePx, 4);
    AppendLittleEndian(bytes, 1, 2);
    AppendLittleEndian(bytes, 24, 2);
    AppendLittleEndian(bytes, 0, std::size_t{24});
    for (std::uint32_t pixel = 0; pixel < kSidePx * kSidePx; ++pixel) {
        bytes.insert(bytes.end(), {static_cast<char>(kColor[2]), static_cast<char>(kColor[1]),
                                   static_cast<char>(kColor[0])});
    }
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(bytes.data(), std::ssize(bytes));
}

std::optional<std::string> PaintableTexture(const assets::Vfs& vfs, assets::LoaderPool& pool) {
    const Result<assets::TrackAssets> track = assets::TrackLoader(vfs, pool).Load(kTrack);
    if (!track) {
        return std::nullopt;
    }
    for (const assets::VfsEntry& entry :
         vfs.List(kTrack).value_or(std::vector<assets::VfsEntry>{})) {
        std::string stem = entry.name.substr(0, entry.name.rfind('.'));
        if (entry.name.ends_with(".BMP") && track->textures.contains(stem)) {
            return stem;
        }
    }
    return std::nullopt;
}

bool IsSolid(const assets::ImageAsset& image) {
    for (std::size_t i = 0; i < image.rgba.size(); i += 4) {
        if (std::to_integer<std::uint8_t>(image.rgba[i]) != kColor[0] ||
            std::to_integer<std::uint8_t>(image.rgba[i + 1]) != kColor[1] ||
            std::to_integer<std::uint8_t>(image.rgba[i + 2]) != kColor[2]) {
            return false;
        }
    }
    return image.widthPx == kSidePx && image.heightPx == kSidePx;
}

void MountPaintMod(const std::filesystem::path& modsFolder, const std::string& texture,
                   assets::Vfs& vfs) {
    std::filesystem::create_directories(modsFolder / "paint");
    std::ofstream(modsFolder / "paint" / "mod.toml") << kManifest;
    WriteSolidBmp(modsFolder / "paint" / "files" / kTrack / (texture + ".BMP"));
    const ModList list = ModList::Discover(modsFolder);
    settings::Registry registry;
    list.DeclareSettings(registry);
    list.MountEnabled(vfs, registry);
}

std::optional<assets::Vfs> RetailVfs() {
    const std::optional<std::filesystem::path> gameFolder = test::GameFolder();
    if (!gameFolder) {
        return std::nullopt;
    }
    Result<assets::JamArchive> archive = assets::JamArchive::Open(*gameFolder / "LEGO.JAM");
    if (!archive) {
        return std::nullopt;
    }
    assets::Vfs vfs;
    vfs.Mount(std::make_unique<assets::JamArchive>(std::move(*archive)));
    return vfs;
}

TEST_CASE("a mod's file replaces a retail track texture") {
    std::optional<assets::Vfs> vfs = RetailVfs();
    if (!vfs) {
        MESSAGE("RR_GAME_PATH isn't set, skipping");
        return;
    }
    assets::LoaderPool pool(1);
    const std::optional<std::string> texture = PaintableTexture(*vfs, pool);
    REQUIRE(texture.has_value());
    const test::TempFolder mods;
    MountPaintMod(mods.Path(), *texture, *vfs);

    const Result<assets::TrackAssets> modded = assets::TrackLoader(*vfs, pool).Load(kTrack);

    REQUIRE(modded.has_value());
    CHECK(IsSolid(*modded->textures.at(*texture)));
}

}
}
