#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace rr::video {

struct YuvPlane {
    std::span<const std::uint8_t> samples;
    std::size_t stride = 0;
};

struct YuvPicture {
    YuvPlane y;
    YuvPlane u;
    YuvPlane v;
    std::size_t width = 0;
    std::size_t height = 0;
    std::size_t chromaShift = 0;
};

/// BT.601 limited range. `rgba` holds `width × height` pixels.
void YuvToRgba(const YuvPicture& picture, std::span<std::uint8_t> rgba);

}
