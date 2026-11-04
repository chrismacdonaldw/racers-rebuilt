#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "app/race_modifiers.hpp"
#include "app/racer/champion_car.hpp"
#include "app/racer/racer_ref.hpp"
#include "assets/game_version.hpp"
#include "core/result.hpp"
#include "settings/racers.hpp"
#include "sim/ai/ai_craft.hpp"
#include "sim/ai/ai_skill.hpp"
#include "sim/powerup/warp_rules.hpp"
#include "sim/race/cheats.hpp"
#include "sim/race/circuit_run.hpp"
#include "sim/race/player_cheats.hpp"
#include "sim/race/race_setup.hpp"
#include "sim/race/respawn.hpp"

namespace rr::app {

inline constexpr int kDefaultLapCount = 3;
inline constexpr std::size_t kDefaultOpponentCount = 5;
inline constexpr int kDefaultAiPace = 100;
inline constexpr int kMinAiPace = 50;
inline constexpr int kMaxAiPace = 150;

/// Players drive grid slots `racer` then `secondRacer`, two alone. Empty `circuit` means home.
/// `playerRacer` and `secondPlayerRacer` resolve `player` and `secondPlayer`. They, `carBody`,
/// `version`, `lagsInput`, `modifiers`, `playerCheats` and `warp` aren't named.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct RaceSceneSetup {
    std::string track;
    std::string circuit;
    sim::RespawnRule respawn;
    std::size_t racer = 0;
    sim::AiDriving aiDriving = sim::AiDriving::Route;
    int aiPace = kDefaultAiPace;
    sim::AiCraft aiCraft;
    std::optional<sim::AiSkill> aiSkill;
    bool isMirrored = false;
    bool isTestDrive = false;
    bool isDemo = false;
    std::size_t players = 1;
    std::size_t secondRacer = 1;
    int lapCount = kDefaultLapCount;
    std::size_t opponents = kDefaultOpponentCount;
    bool isTimeTrial = false;
    std::size_t firstSlot = 0;
    RacerRef player;
    std::optional<settings::Racer> playerRacer;
    RacerRef secondPlayer;
    std::optional<settings::Racer> secondPlayerRacer;
    CarBody carBody = CarBody::Bricks;
    assets::GameVersion version = assets::GameVersion::Release1999;
    bool lagsInput = false;
    sim::Cheats cheats;
    RaceModifiers modifiers;
    sim::PlayerCheats playerCheats;
    sim::WarpRules warp;

    friend bool operator==(const RaceSceneSetup&, const RaceSceneSetup&) = default;
};

/// A lone player races `opponents` computer racers, and two players race alone.
[[nodiscard]] std::size_t RacerCount(const RaceSceneSetup& setup, std::size_t gridSize);

/// LNFRRRM also mirrors a race named as the Rocket Racer circuit's, and the mirror modifier any.
[[nodiscard]] bool LoadsMirrored(const RaceSceneSetup& setup);

/// Such as `race:RACEC0R0:respawn=3000:racer=2`, leaving out fields still at their defaults.
[[nodiscard]] std::string RaceSceneName(const RaceSceneSetup& setup);

[[nodiscard]] bool IsRaceScene(std::string_view scene);

[[nodiscard]] RacerRef ScenePlayer(std::string_view scene);
[[nodiscard]] RacerRef SceneSecondPlayer(std::string_view scene);

/// Names `player` in each race, circuit or time trial scene of a `>` chain.
[[nodiscard]] std::string WithPlayer(std::string_view scenes, const RacerRef& player);

/// Replays before format 7 drove physics racers with every craft skill, so their scenes gain it.
[[nodiscard]] std::string UpgradedScene(std::string_view scenes, std::uint16_t formatVersion);

/// Names player 2's racer in each versus race of a `>` chain.
[[nodiscard]] std::string WithSecondPlayer(std::string_view scenes, const RacerRef& player);

[[nodiscard]] Result<RaceSceneSetup> ParseRaceScene(std::string_view scene);

[[nodiscard]] bool IsTrialScene(std::string_view scene);

/// A time trial races the player alone over 3 laps.
[[nodiscard]] Result<RaceSceneSetup> ParseTrialScene(std::string_view scene);

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
