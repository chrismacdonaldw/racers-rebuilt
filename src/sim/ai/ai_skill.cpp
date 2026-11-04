#include "sim/ai/ai_skill.hpp"

#include <algorithm>

namespace rr::sim {
namespace {

constexpr std::array kProfiles{
    SkillProfile{
        .speedShare = 0.90F, .brakeMargin = 1.02F, .wobble = 2.0F, .reaction = Milliseconds{400}},
    SkillProfile{
        .speedShare = 0.95F, .brakeMargin = 1.06F, .wobble = 1.2F, .reaction = Milliseconds{300}},
    SkillProfile{
        .speedShare = 1.00F, .brakeMargin = 1.10F, .wobble = 0.6F, .reaction = Milliseconds{200}},
    SkillProfile{.speedShare = 1.08F,
                 .brakeMargin = 1.18F,
                 .wobble = 0.0F,
                 .reaction = Milliseconds{60},
                 .takesShortcuts = true}};

constexpr float kFarAheadLaps = 0.1F;
constexpr char kOriginalMark = 'o';
constexpr char kEqualMark = 'e';
constexpr char kCustomMark = 'c';

int ClampedLevel(int level) {
    return std::clamp(level, kEasiestLevel, kExpertLevel);
}

int PaceDrop(assets::RoutePace pace) {
    switch (pace) {
        case assets::RoutePace::Fast:
            return 0;
        case assets::RoutePace::Medium:
            return 1;
        case assets::RoutePace::Slow:
            return 2;
    }
    return 0;
}

std::optional<int> LevelDigit(char digit) {
    const int level = digit - '0';
    return level >= kEasiestLevel && level <= kExpertLevel ? std::optional(level) : std::nullopt;
}

std::optional<AiSkill> ParseCustom(std::string_view digits) {
    AiSkill skill{.spread = SkillSpread::Custom};
    if (digits.size() != kComputerSlots) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < kComputerSlots; ++i) {
        const std::optional<int> level = LevelDigit(digits[i]);
        if (!level) {
            return std::nullopt;
        }
        skill.slotLevels.at(i) = *level;
    }
    return skill;
}

}

const SkillProfile& ProfileFor(int level) {
    return kProfiles.at(static_cast<std::size_t>(ClampedLevel(level) - kEasiestLevel));
}

int LevelFor(const AiSkill& skill, std::size_t gridSlot, assets::RoutePace pace) {
    switch (skill.spread) {
        case SkillSpread::Original:
            return ClampedLevel(skill.difficulty - PaceDrop(pace));
        case SkillSpread::Equal:
            return ClampedLevel(skill.difficulty);
        case SkillSpread::Custom:
            return ClampedLevel(
                skill.slotLevels.at(std::clamp<std::size_t>(gridSlot, 1, kComputerSlots) - 1));
    }
    return kChampionLevel;
}

std::string SkillText(const AiSkill& skill) {
    if (skill.spread != SkillSpread::Custom) {
        const char mark = skill.spread == SkillSpread::Original ? kOriginalMark : kEqualMark;
        return {mark, static_cast<char>('0' + skill.difficulty)};
    }
    std::string text(1, kCustomMark);
    for (const int level : skill.slotLevels) {
        text += static_cast<char>('0' + level);
    }
    return text;
}

std::optional<AiSkill> ParseSkillText(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    if (text.front() == kCustomMark) {
        return ParseCustom(text.substr(1));
    }
    if ((text.front() != kOriginalMark && text.front() != kEqualMark) || text.size() != 2) {
        return std::nullopt;
    }
    const std::optional<int> level = LevelDigit(text[1]);
    if (!level) {
        return std::nullopt;
    }
    return AiSkill{
        .spread = text.front() == kOriginalMark ? SkillSpread::Original : SkillSpread::Equal,
        .difficulty = *level};
}

int CatchUpShift(std::span<const PacedRacer> racers, std::size_t racer) {
    std::optional<float> best;
    for (const PacedRacer& paced : racers) {
        if (!paced.isAiControlled) {
            best = std::max(best.value_or(paced.lapProgress), paced.lapProgress);
        }
    }
    if (!best) {
        return 0;
    }
    const float progress = racers[racer].lapProgress;
    if (progress < *best) {
        return 1;
    }
    return progress > *best + kFarAheadLaps ? -1 : 0;
}

}
