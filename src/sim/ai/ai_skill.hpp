#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "assets/race_route.hpp"
#include "core/duration.hpp"
#include "sim/ai/rubber_band.hpp"

namespace rr::sim {

inline constexpr int kEasiestLevel = 1;
inline constexpr int kChampionLevel = 3;
inline constexpr int kExpertLevel = 4;
inline constexpr std::size_t kComputerSlots = 5;

/// How a physics racer at one level drives, tuned against the original's route playback.
struct SkillProfile {
    float speedShare = 1.0F;
    float brakeMargin = 1.1F;
    float wobble = 0.0F;
    Milliseconds reaction{0};
    bool takesShortcuts = false;
};

/// `level` is clamped to 1..4.
[[nodiscard]] const SkillProfile& ProfileFor(int level);

enum class SkillSpread : std::uint8_t { Original, Equal, Custom };

struct AiSkill {
    SkillSpread spread = SkillSpread::Original;
    int difficulty = kChampionLevel;
    std::array<int, kComputerSlots> slotLevels{kChampionLevel, kChampionLevel, kChampionLevel,
                                               kChampionLevel, kChampionLevel};
    bool catchesUp = false;

    friend bool operator==(const AiSkill&, const AiSkill&) = default;
};

/// Original gives F routes the difficulty, M one less and S two less, so the boss leads.
[[nodiscard]] int LevelFor(const AiSkill& skill, std::size_t gridSlot, assets::RoutePace pace);

/// `o3` or `e4` for Original or Equal at a difficulty, `c12344` for Custom slot levels.
[[nodiscard]] std::string SkillText(const AiSkill& skill);

/// Leaves `catchesUp` false. Fails on any other shape or a level outside 1..4.
[[nodiscard]] std::optional<AiSkill> ParseSkillText(std::string_view text);

/// One level up behind the best player, one down a tenth of a lap ahead of them, else 0.
[[nodiscard]] int CatchUpShift(std::span<const PacedRacer> racers, std::size_t racer);

}
