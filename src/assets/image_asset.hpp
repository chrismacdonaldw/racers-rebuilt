#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rr::assets {

/// RGBA8, top row first. `enhanced*` is the smooth textures look; empty `enhancedRgba` uses `rgba`.
struct ImageAsset {
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;
    std::vector<std::byte> rgba;
    std::uint32_t mipLevels = 1;
    std::vector<std::byte> enhancedRgba;
    std::uint32_t enhancedMipLevels = 1;
};

[[nodiscard]] inline bool HasEnhancedLook(const ImageAsset& image) {
    return !image.enhancedRgba.empty() || image.enhancedMipLevels > image.mipLevels;
}

/// Levels from the full size down to 1x1.
[[nodiscard]] constexpr std::uint32_t FullMipLevels(std::uint32_t widthPx, std::uint32_t heightPx) {
    return static_cast<std::uint32_t>(std::bit_width(std::max({widthPx, heightPx, 1U})));
}

}
