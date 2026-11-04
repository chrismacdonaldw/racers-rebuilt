#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"
#include "sim/ai/ai_skill.hpp"

namespace rr::app {

inline constexpr std::string_view kAiSkillCategory = "Enhancements/Gameplay";
inline constexpr std::array<std::string_view, 3> kSkillSpreadNames{"Original", "Equal", "Custom"};
inline constexpr settings::SettingRange<int> kLevelRange{.min = sim::kEasiestLevel,
                                                         .max = sim::kMasterLevel};

inline constexpr settings::SettingSpec<bool> kAiSkill{
    .key = "race.ai_skill",
    .defaultValue = false,
    .label = "Physics racers race at a skill level",
    .category = kAiSkillCategory,
    .tooltip =
        "Replaces the physics racers' pace with levels 1 to 5. Level 3 drives like a "
        "typical boss of the original."};

inline constexpr settings::SettingRequirement kNeedsSkill{.key = kAiSkill.key, .choice = "true"};

inline constexpr settings::SettingSpec<int> kAiDifficulty{
    .key = "race.ai_difficulty",
    .defaultValue = sim::kChampionLevel,
    .range = kLevelRange,
    .label = "Computer racers' level (3 is a typical boss)",
    .category = kAiSkillCategory,
    .requirement = kNeedsSkill};

inline constexpr settings::SettingSpec<std::string_view> kAiSpread{
    .key = "race.ai_spread",
    .defaultValue = kSkillSpreadNames[0],
    .label = "How levels spread over the grid",
    .category = kAiSkillCategory,
    .choices = kSkillSpreadNames,
    .tooltip =
        "Original gives fast-route racers and the boss the level, medium ones one less and "
        "slow ones two less. Equal gives everyone the level. Custom sets each grid slot.",
    .requirement = kNeedsSkill};

inline constexpr settings::SettingRequirement kNeedsCustomSpread{.key = kAiSpread.key,
                                                                 .choice = kSkillSpreadNames[2]};

[[nodiscard]] constexpr settings::SettingSpec<int> SlotLevelSpec(std::string_view key,
                                                                 std::string_view label) {
    return {.key = key,
            .defaultValue = sim::kChampionLevel,
            .range = kLevelRange,
            .label = label,
            .category = kAiSkillCategory,
            .requirement = kNeedsCustomSpread};
}

inline constexpr std::array<settings::SettingSpec<int>, sim::kComputerSlots> kAiSlotLevels{
    SlotLevelSpec("race.ai_level_slot1", "Grid slot 1 level"),
    SlotLevelSpec("race.ai_level_slot2", "Grid slot 2 level"),
    SlotLevelSpec("race.ai_level_slot3", "Grid slot 3 level"),
    SlotLevelSpec("race.ai_level_slot4", "Grid slot 4 level"),
    SlotLevelSpec("race.ai_level_slot5", "Grid slot 5 level")};

inline constexpr settings::SettingSpec<bool> kAiFairCatchUp{
    .key = "race.ai_fair_catch_up",
    .defaultValue = false,
    .label = "Skilled racers catch up by driving better",
    .category = kAiSkillCategory,
    .tooltip =
        "After 15 s, a racer behind the best player drives one level higher and one a "
        "tenth of a lap ahead drives one level lower. No speed bonus.",
    .requirement = kNeedsSkill};

inline void DeclareAiSkillSettings(settings::Registry& registry) {
    registry.Declare(kAiSkill);
    registry.Declare(kAiDifficulty);
    registry.Declare(kAiSpread);
    for (const settings::SettingSpec<int>& slot : kAiSlotLevels) {
        registry.Declare(slot);
    }
    registry.Declare(kAiFairCatchUp);
}

[[nodiscard]] inline sim::SkillSpread SpreadFrom(const settings::Registry& registry) {
    const std::string spread = registry.Get(kAiSpread);
    if (spread == kSkillSpreadNames[2]) {
        return sim::SkillSpread::Custom;
    }
    return spread == kSkillSpreadNames[1] ? sim::SkillSpread::Equal : sim::SkillSpread::Original;
}

[[nodiscard]] inline std::optional<sim::AiSkill> AiSkillFrom(const settings::Registry& registry) {
    if (!registry.Get(kAiSkill)) {
        return std::nullopt;
    }
    sim::AiSkill skill{.spread = SpreadFrom(registry),
                       .difficulty = registry.Get(kAiDifficulty),
                       .catchesUp = registry.Get(kAiFairCatchUp)};
    for (std::size_t i = 0; i < sim::kComputerSlots; ++i) {
        skill.slotLevels.at(i) = registry.Get(kAiSlotLevels.at(i));
    }
    return skill;
}

}
