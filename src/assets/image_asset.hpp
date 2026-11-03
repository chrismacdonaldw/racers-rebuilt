#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rr::assets {

/// `rgba` holds `widthPx * heightPx` RGBA8 pixels, top row first.
struct ImageAsset {
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;
    std::vector<std::byte> rgba;
    std::uint32_t mipLevels = 1;
};

/// Levels from the full size down to 1x1.
[[nodiscard]] constexpr std::uint32_t FullMipLevels(std::uint32_t widthPx, std::uint32_t heightPx) {
    return static_cast<std::uint32_t>(std::bit_width(std::max({widthPx, heightPx, 1U})));
}

}
