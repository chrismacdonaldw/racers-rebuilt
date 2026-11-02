#pragma once

#include <array>
#include <span>
#include <string>

#include "core/result.hpp"

namespace rr::app {

inline constexpr std::size_t kShownPlaces = 8;

struct HudStrings {
    std::array<std::u16string, kShownPlaces> placeSuffixes;
    std::u16string gap;
    std::u16string wrongWay;
    std::u16string best;
    std::u16string lap;
    std::u16string finish;
    std::u16string go;
    std::u16string finished;
    std::u16string stillRacing;
    std::u16string circuit;
    std::u16string paused;
    std::u16string resume;
    std::u16string restart;
    std::u16string restartCircuit;
    std::u16string quit;
};

/// Picks the HUD and pause labels out of a language's `GAME.SRF` strings.
[[nodiscard]] Result<HudStrings> ReadHudStrings(std::span<const std::u16string> gameStrings);

}
