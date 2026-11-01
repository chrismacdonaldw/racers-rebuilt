#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "core/result.hpp"
#include "sim/race/race_setup.hpp"
#include "sim/race/respawn.hpp"

namespace rr::app {

/// Slot `racer`'s prebuilt racer swaps into slot 0 for the player. Empty `circuit` means home.
struct RaceSceneSetup {
    std::string track;
    std::string circuit;
    sim::RespawnRule respawn;
    std::size_t racer = 0;
    sim::AiDriving aiDriving = sim::AiDriving::Route;

    friend bool operator==(const RaceSceneSetup&, const RaceSceneSetup&) = default;
};

/// The name replays record, such as `race:RACEC0R0:respawn=3000:racer=2`, with `respawn=off`
/// when respawning is off. `racer` may be left out, and `circuit=c1` and `ai=physics` are added
/// only when set.
[[nodiscard]] std::string RaceSceneName(const RaceSceneSetup& setup);

[[nodiscard]] bool IsRaceScene(std::string_view scene);
[[nodiscard]] Result<RaceSceneSetup> ParseRaceScene(std::string_view scene);

}
