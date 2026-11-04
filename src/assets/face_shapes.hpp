#pragma once

#include <cstdint>
#include <vector>

#include "assets/face_drawing.hpp"

namespace rr::assets {

/// One byte per pixel, top row first. A pixel is inside when at least half its 4x4 samples are.
[[nodiscard]] std::vector<std::uint8_t> ShapeMask(const FaceShape& shape);

}
