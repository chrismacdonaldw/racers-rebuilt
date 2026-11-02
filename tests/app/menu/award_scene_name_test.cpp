#include "app/menu/award_scene_name.hpp"

#include <doctest/doctest.h>

#include <optional>

namespace rr::app {
namespace {

using sim::CircuitAward;

TEST_CASE("an award scene name names its award") {
    for (const CircuitAward award :
         {CircuitAward::Gold, CircuitAward::Silver, CircuitAward::Bronze, CircuitAward::None}) {
        const AwardSceneSetup setup{.circuit = "c6", .award = award};
        CHECK(ParseAwardScene(AwardSceneName(setup)) == setup);
    }
}

TEST_CASE("only award names parse as award scenes") {
    CHECK(AwardSceneName({.circuit = "c0", .award = CircuitAward::Silver}) ==
          "menu:award:c0:silver");
    CHECK(IsAwardScene("menu:award:c0:none"));
    CHECK_FALSE(ParseAwardScene("menu:award:gold").has_value());
    CHECK_FALSE(ParseAwardScene("menu:award::gold").has_value());
    CHECK_FALSE(ParseAwardScene("menu:award:c0:platinum").has_value());
    CHECK_FALSE(ParseAwardScene("menu:main").has_value());
}

TEST_CASE("each award plays its screen, then a car win screen for a new part set") {
    CHECK(AwardScreenName(CircuitAward::Gold, 0) == "screen:c_award1>screen:wincar");
    CHECK(AwardScreenName(CircuitAward::Gold, 6) == "screen:c_award1>screen:winrrcar");
    CHECK(AwardScreenName(CircuitAward::Gold, std::nullopt) == "screen:c_award1");
    CHECK(AwardScreenName(CircuitAward::Silver, std::nullopt) == "screen:c_award2");
    CHECK(AwardScreenName(CircuitAward::Bronze, std::nullopt) == "screen:c_award3");
    CHECK(AwardScreenName(CircuitAward::None, std::nullopt) == "screen:c_award4");
}

}
}
