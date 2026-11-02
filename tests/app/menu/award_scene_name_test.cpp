#include "app/menu/award_scene_name.hpp"

#include <doctest/doctest.h>

#include <optional>

namespace rr::app {
namespace {

using sim::CircuitAward;

TEST_CASE("an award scene name names its award") {
    for (const CircuitAward award :
         {CircuitAward::Gold, CircuitAward::Silver, CircuitAward::Bronze, CircuitAward::None}) {
        const AwardSceneSetup setup{.circuit = "c6", .award = award, .racer = 3};
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
    CHECK_FALSE(ParseAwardScene("menu:award:c0:gold:racer=x").has_value());
    CHECK_FALSE(ParseAwardScene("menu:main").has_value());
}

TEST_CASE("an award names its racer, and the car win the champion") {
    const AwardDrivers drivers{.player = "JT", .champion = "CR"};

    CHECK(AwardScreenName(CircuitAward::Gold, 0, drivers) ==
          "screen:c_award1:driver=JT>screen:wincar:driver=CR");
    CHECK(AwardScreenName(CircuitAward::Gold, 6, drivers) ==
          "screen:c_award1:driver=JT>screen:winrrcar:driver=JT");
    CHECK(AwardScreenName(CircuitAward::Gold, std::nullopt, drivers) ==
          "screen:c_award1:driver=JT");
    CHECK(AwardScreenName(CircuitAward::Silver, std::nullopt, {}) == "screen:c_award2");
    CHECK(AwardScreenName(CircuitAward::Bronze, std::nullopt, {}) == "screen:c_award3");
    CHECK(AwardScreenName(CircuitAward::None, std::nullopt, {}) == "screen:c_award4");
}

}
}
