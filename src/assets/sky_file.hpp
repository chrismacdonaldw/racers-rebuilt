#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/duration.hpp"
#include "core/result.hpp"

namespace rr::assets {

using Rgb8 = std::array<std::uint8_t, 3>;

inline constexpr Rgb8 kWhiteRgb{255, 255, 255};

struct SkyKeyframe {
    Milliseconds length{1000};
    Rgb8 horizon = kWhiteRgb;
    Rgb8 middle = kWhiteRgb;
    Rgb8 top = kWhiteRgb;
};

struct SkyGradient {
    std::string name;
    std::vector<SkyKeyframe> keyframes;
};

/// `heightOffset` raises the dome, in game units.
struct SkyFile {
    std::vector<SkyGradient> skies;
    std::size_t initial = 0;
    float heightOffset = 0.0F;
};

/// Compares case-insensitively on the first 8 characters, as the original's name buffers hold.
[[nodiscard]] bool IsSameSkyName(std::string_view first, std::string_view second);

/// Reads a `BACKGRND.SKB`. Fails without skies, or with a sky that has no keyframes.
[[nodiscard]] Result<SkyFile> ParseSkyFile(std::span<const std::byte> bytes);

}
