#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "assets/image_asset.hpp"
#include "assets/texture_database.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

using GameFileVisitor = std::function<Result<void>(const std::string& gamePath)>;

/// Visits a folder's files before its subfolders, each in name order.
[[nodiscard]] Result<void> ForEachGameFile(const assets::Vfs& vfs, const GameFileVisitor& visit);

/// Only `.BMP` and `.TGA` paths, in upper case, have a kind.
[[nodiscard]] std::optional<assets::TextureFileKind> ImageKindOf(std::string_view gamePath);

/// `decode` returns nothing for a file it skips, and an error for an image that doesn't decode.
struct ImageDump {
    std::function<Result<std::optional<assets::ImageAsset>>(const std::string& gamePath)> decode;
    std::function<std::filesystem::path(const std::string& gamePath)> target;
};

struct DumpCounts {
    std::size_t saved = 0;
    std::size_t failed = 0;
};

[[nodiscard]] Result<DumpCounts> DumpEachImage(const assets::Vfs& vfs, const ImageDump& dump);

/// Writes each BMP and TGA to `<folder>/<game path>.png`, skipping any that don't decode.
[[nodiscard]] Result<void> DumpImages(const assets::Vfs& vfs, const std::filesystem::path& folder);

}
