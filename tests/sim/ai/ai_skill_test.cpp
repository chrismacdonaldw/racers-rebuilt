#include "sim/ai/ai_skill.hpp"

#include <doctest/doctest.h>

#include <array>
#include <functional>
#include <string_view>

namespace rr::sim {
namespace {

using assets::RoutePace;

template <typename Field, typename Order>
bool RunsUpTheLevels(Field field, Order order) {
    for (int level = kEasiestLevel; level < kMasterLevel; ++level) {
        if (!order(std::invoke(field, ProfileFor(level + 1)),
                   std::invoke(field, ProfileFor(level)))) {
            return false;
        }
    }
    return true;
}

TEST_CASE("each skill level drives faster, brakes later and wanders less than the one below") {
    CHECK(RunsUpTheLevels(&SkillProfile::speedShare, std::greater{}));
    CHECK(RunsUpTheLevels(&SkillProfile::brakeMargin, std::greater{}));
    CHECK(RunsUpTheLevels(&SkillProfile::wobble, std::less{}));
    CHECK(RunsUpTheLevels(&SkillProfile::reaction, std::less{}));
    CHECK(ProfileFor(kChampionLevel).speedShare == 1.0F);
    CHECK_FALSE(ProfileFor(kChampionLevel).takesShortcuts);
    CHECK(ProfileFor(kExpertLevel).takesShortcuts);
    CHECK(&ProfileFor(0) == &ProfileFor(kEasiestLevel));
    CHECK(&ProfileFor(9) == &ProfileFor(kMasterLevel));
}

TEST_CASE("the original spread drops a level for a medium route and two for a slow one") {
    const AiSkill skill{.difficulty = 4};

    CHECK(LevelFor(skill, 1, RoutePace::Fast) == 4);
    CHECK(LevelFor(skill, 2, RoutePace::Medium) == 3);
    CHECK(LevelFor(skill, 3, RoutePace::Slow) == 2);
    CHECK(LevelFor({.difficulty = 1}, 3, RoutePace::Slow) == kEasiestLevel);
}

TEST_CASE("the equal spread gives every slot the difficulty, and custom each slot its own") {
    const AiSkill equal{.spread = SkillSpread::Equal, .difficulty = 5};
    const AiSkill custom{.spread = SkillSpread::Custom, .slotLevels = {1, 2, 3, 4, 5}};

    CHECK(LevelFor(equal, 3, RoutePace::Slow) == 5);
    CHECK(LevelFor(custom, 1, RoutePace::Slow) == 1);
    CHECK(LevelFor(custom, 5, RoutePace::Fast) == 5);
    CHECK(LevelFor(custom, 0, RoutePace::Fast) == 1);
}

TEST_CASE("skill text names the spread and its levels and reads back") {
    const AiSkill original{.difficulty = 3};
    const AiSkill equal{.spread = SkillSpread::Equal, .difficulty = 5};
    const AiSkill custom{.spread = SkillSpread::Custom, .slotLevels = {1, 2, 3, 4, 5}};

    CHECK(SkillText(original) == "o3");
    CHECK(SkillText(equal) == "e5");
    CHECK(SkillText(custom) == "c12345");
    CHECK(ParseSkillText("o3") == original);
    CHECK(ParseSkillText("e5") == equal);
    CHECK(ParseSkillText("c12345") == custom);
}

TEST_CASE("skill text rejects other shapes and levels outside 1 to 5") {
    for (const std::string_view text : {"", "o", "o0", "o6", "x3", "o33", "c1234", "c12346"}) {
        CAPTURE(text);
        CHECK_FALSE(ParseSkillText(text).has_value());
    }
}

TEST_CASE("catch-up lifts a racer behind the best player and eases one far ahead") {
    const std::array racers{PacedRacer{.lapProgress = 1.0F},
                            PacedRacer{.lapProgress = 0.5F, .isAiControlled = true},
                            PacedRacer{.lapProgress = 1.05F, .isAiControlled = true},
                            PacedRacer{.lapProgress = 1.2F, .isAiControlled = true}};
    const std::array computers{PacedRacer{.lapProgress = 1.0F, .isAiControlled = true}};

    CHECK(CatchUpShift(racers, 1) == 1);
    CHECK(CatchUpShift(racers, 2) == 0);
    CHECK(CatchUpShift(racers, 3) == -1);
    CHECK(CatchUpShift(computers, 0) == 0);
}

}
}
