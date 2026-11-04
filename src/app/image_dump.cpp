#include "app/image_dump.hpp"

#include <format>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "platform/image_file.hpp"

namespace rr::app {
namespace {

Result<void> SavePngAt(const std::filesystem::path& target, const assets::ImageAsset& image) {
    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);
    if (error) {
        return Fail(ErrorCode::Io,
                    std::format("can't create {}: {}", Utf8FromPath(target.parent_path()),
                                error.message()));
    }
    return platform::SavePng(target, image.widthPx, image.heightPx, image.rgba);
}

Result<std::optional<assets::ImageAsset>> DecodeGameImage(const assets::Vfs& vfs,
                                                          const std::string& gamePath) {
    const std::optional<assets::TextureFileKind> kind = ImageKindOf(gamePath);
    if (!kind) {
        return std::nullopt;
    }
    return vfs.Read(gamePath)
        .and_then(
            [&](const assets::FileData& data) { return assets::DecodeImage(data.Bytes(), *kind); })
        .transform([](assets::ImageAsset image) { return std::optional(std::move(image)); });
}

}

Result<void> ForEachGameFile(const assets::Vfs& vfs, const GameFileVisitor& visit) {
    std::vector<std::string> pending{""};
    while (!pending.empty()) {
        const std::string folder = std::move(pending.back());
        pending.pop_back();
        Result<std::vector<assets::VfsEntry>> entries = vfs.List(folder);
        if (!entries) {
            return std::unexpected(std::move(entries.error()));
        }
        std::vector<std::string> folders;
        for (const assets::VfsEntry& entry : *entries) {
            std::string gamePath = assets::JoinGamePath(folder, entry.name);
            if (entry.kind == assets::EntryKind::Directory) {
                folders.push_back(std::move(gamePath));
                continue;
            }
            Result<void> visited = visit(gamePath);
            if (!visited) {
                return visited;
            }
        }
        pending.insert(pending.end(), std::make_move_iterator(folders.rbegin()),
                       std::make_move_iterator(folders.rend()));
    }
    return {};
}

std::optional<assets::TextureFileKind> ImageKindOf(std::string_view gamePath) {
    if (gamePath.ends_with(".BMP")) {
        return assets::TextureFileKind::Bmp;
    }
    if (gamePath.ends_with(".TGA")) {
        return assets::TextureFileKind::Tga;
    }
    return std::nullopt;
}

Result<DumpCounts> DumpEachImage(const assets::Vfs& vfs, const ImageDump& dump) {
    DumpCounts counts;
    Result<void> walked = ForEachGameFile(vfs, [&](const std::string& gamePath) -> Result<void> {
        Result<std::optional<assets::ImageAsset>> image = dump.decode(gamePath);
        if (!image) {
            log::Warn("{}: {}", gamePath, image.error().message);
            ++counts.failed;
            return {};
        }
        if (!*image) {
            return {};
        }
        return SavePngAt(dump.target(gamePath), **image).transform([&] { ++counts.saved; });
    });
    return walked.transform([&] { return counts; });
}

Result<void> DumpImages(const assets::Vfs& vfs, const std::filesystem::path& folder) {
    const ImageDump dump{
        .decode = [&](const std::string& gamePath) { return DecodeGameImage(vfs, gamePath); },
        .target =
            [&](const std::string& gamePath) { return folder / PathFromUtf8(gamePath + ".png"); }};
    return DumpEachImage(vfs, dump).transform([&](const DumpCounts& counts) {
        log::Info("dumped {} images to {}, {} didn't decode", counts.saved, Utf8FromPath(folder),
                  counts.failed);
    });
}

}
