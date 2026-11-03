#include "app/menu/race_preview.hpp"

#include <array>
#include <cstddef>

#include "assets/race_list.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kTintedCircuits = 7;
constexpr std::size_t kFinalTint = 4;

constexpr render::Color Rgb(int r, int g, int b) {
    constexpr float kMax = 255.0F;
    return {.r = static_cast<float>(r) / kMax,
            .g = static_cast<float>(g) / kMax,
            .b = static_cast<float>(b) / kMax};
}

constexpr std::array kTints{Rgb(173, 173, 173), Rgb(255, 223, 0), Rgb(230, 220, 180),
                            Rgb(37, 61, 223)};
constexpr render::Color kFinalTint1999 = Rgb(255, 218, 31);
constexpr render::Color kFinalTint2001 = Rgb(218, 31, 31);

using CircuitTints = std::array<std::size_t, assets::kCircuitRaces>;
constexpr std::array<CircuitTints, kTintedCircuits> kCircuitTints{{{1, 0, 3, 2},
                                                                   {1, 0, 3, 2},
                                                                   {0, 1, 2, 3},
                                                                   {2, 0, 3, 1},
                                                                   {2, 0, 3, 1},
                                                                   {3, 1, 2, 0},
                                                                   {4, 0, 0, 0}}};

}

std::optional<render::Color> FrameTintOf(const RacePreview& preview,
                                         const sim::VersionRules& rules) {
    const auto position = static_cast<std::size_t>(preview.position);
    if (preview.circuit >= kTintedCircuits || position >= assets::kCircuitRaces) {
        return std::nullopt;
    }
    const std::size_t tint = kCircuitTints.at(preview.circuit).at(position);
    if (tint == kFinalTint) {
        return rules.tintsFinalFrameRed ? kFinalTint2001 : kFinalTint1999;
    }
    return kTints.at(tint);
}

}
