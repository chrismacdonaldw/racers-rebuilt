#include "app/menu/award_scene_name.hpp"

#include <doctest/doctest.h>

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

}
}
