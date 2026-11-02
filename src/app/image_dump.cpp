#include "app/image_dump.hpp"

#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "assets/bmp_image.hpp"
#include "assets/game_path.hpp"
#include "assets/image_asset.hpp"
#include "assets/tga_image.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "platform/image_file.hpp"

namespace rr::app {
namespace {

using Decoder = Result<assets::ImageAsset> (*)(std::span<const std::byte> bytes);

struct DumpCounts {
    std::size_t saved = 0;
    std::size_t failed = 0;
};

Decoder DecoderFor(std::string_view name) {
    if (name.ends_with(".BMP")) {
        return assets::DecodeBmp;
    }
    if (name.ends_with(".TGA")) {
        return assets::DecodeTga;
    }
    return nullptr;
}

Result<void> DumpImage(const assets::Vfs& vfs, const std::string& gamePath, Decoder decode,
                       const std::filesystem::path& target, DumpCounts& counts) {
    Result<assets::ImageAsset> image = vfs.Read(gamePath).and_then(
        [&](const assets::FileData& data) { return decode(data.Bytes()); });
    if (!image) {
        log::Warn("{}: {}", gamePath, image.error().message);
        ++counts.failed;
        return {};
    }
    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);
    if (error) {
        return Fail(ErrorCode::Io,
                    std::format("can't create {}: {}", Utf8FromPath(target.parent_path()),
                                error.message()));
    }
    return platform::SavePng(target, image->widthPx, image->heightPx, image->rgba).transform([&] {
        ++counts.saved;
    });
}

Result<void> DumpFolder(const assets::Vfs& vfs, const std::string& gameFolder,
                        const std::filesystem::path& folder, std::vector<std::string>& pending,
                        DumpCounts& counts) {
    Result<std::vector<assets::VfsEntry>> entries = vfs.List(gameFolder);
    if (!entries) {
        return std::unexpected(std::move(entries.error()));
    }
    for (const assets::VfsEntry& entry : *entries) {
        std::string gamePath = assets::JoinGamePath(gameFolder, entry.name);
        const Decoder decode = DecoderFor(entry.name);
        if (entry.kind == assets::EntryKind::Directory) {
            pending.push_back(std::move(gamePath));
            continue;
        }
        if (decode == nullptr) {
            continue;
        }
        Result<void> dumped =
            DumpImage(vfs, gamePath, decode, folder / PathFromUtf8(gamePath + ".png"), counts);
        if (!dumped) {
            return dumped;
        }
    }
    return {};
}

}

Result<void> DumpImages(const assets::Vfs& vfs, const std::filesystem::path& folder) {
    DumpCounts counts;
    std::vector<std::string> pending{""};
    while (!pending.empty()) {
        const std::string gameFolder = std::move(pending.back());
        pending.pop_back();
        Result<void> dumped = DumpFolder(vfs, gameFolder, folder, pending, counts);
        if (!dumped) {
            return dumped;
        }
    }
    log::Info("dumped {} images to {}, {} didn't decode", counts.saved, Utf8FromPath(folder),
              counts.failed);
    return {};
}

}
