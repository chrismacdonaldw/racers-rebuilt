#pragma once

#include <array>
#include <cstdint>

namespace rr {

using Rgba8 = std::array<std::uint8_t, 4>;

inline constexpr Rgba8 kOpaqueWhite{255, 255, 255, 255};

}
