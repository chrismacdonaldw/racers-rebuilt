#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"

namespace rr::assets {

/// `ticks` are progress marks as fractions of the screen's width and height.
struct LoadingScreenAsset {
    std::string image;
    std::int32_t nameIndex = 0;
    std::vector<std::array<float, 2>> ticks;
};

[[nodiscard]] Result<LoadingScreenAsset> ParseLsb(std::span<const std::byte> bytes);

}
