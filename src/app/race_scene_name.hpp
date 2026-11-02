#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "core/result.hpp"
#include "sim/race/circuit_run.hpp"
#include "sim/race/race_setup.hpp"
#include "sim/race/respawn.hpp"

namespace rr::app {

/// Slot `racer`'s racer swaps into slot 0. Empty `circuit` means home. Two `players` race alone.
struct RaceSceneSetup {
    std::string track;
    std::string circuit;
    sim::RespawnRule respawn;
    std::size_t racer = 0;
    sim::AiDriving aiDriving = sim::AiDriving::Route;
    bool isMirrored = false;
    std::size_t players = 1;

    friend bool operator==(const RaceSceneSetup&, const RaceSceneSetup&) = default;
};

/// Such as `race:RACEC0R0:respawn=3000:racer=2`, leaving out fields still at their defaults.
[[nodiscard]] std::string RaceSceneName(const RaceSceneSetup& setup);

[[nodiscard]] bool IsRaceScene(std::string_view scene);
[[nodiscard]] Result<RaceSceneSetup> ParseRaceScene(std::string_view scene);

/// `race.track` stays empty: each race takes its track from `race.circuit`.
struct CircuitSceneSetup {
    RaceSceneSetup race;
    sim::CircuitStanding standing;

    friend bool operator==(const CircuitSceneSetup&, const CircuitSceneSetup&) = default;
};

/// Such as `circuit:c0:respawn=3000:racer=2:race=1:points=30,20,10,3,2,1`.
[[nodiscard]] std::string CircuitSceneName(const CircuitSceneSetup& setup);

[[nodiscard]] bool IsCircuitScene(std::string_view scene);
[[nodiscard]] Result<CircuitSceneSetup> ParseCircuitScene(std::string_view scene);

}
