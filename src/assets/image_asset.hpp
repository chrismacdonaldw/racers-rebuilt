#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rr::assets {

/// `rgba` holds `widthPx * heightPx` RGBA8 pixels, top row first.
struct ImageAsset {
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;
    std::vector<std::byte> rgba;
};

}
