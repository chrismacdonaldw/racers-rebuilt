#pragma once

#include <optional>
#include <string_view>

#include "assets/image_asset.hpp"
#include "assets/texture_database.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

struct UiPixels {
    assets::ImageAsset pixels;
    std::optional<assets::ImageAsset> original;

    /// The image UI layout measures, which stays the original when a PNG replaced it.
    [[nodiscard]] const assets::ImageAsset& Layout() const { return original ? *original : pixels; }
};

/// `original` is set only when the read served a PNG in place of the game image.
[[nodiscard]] Result<UiPixels> ReadUiPixels(const assets::Vfs& vfs, std::string_view path,
                                            assets::TextureFileKind kind);

}
