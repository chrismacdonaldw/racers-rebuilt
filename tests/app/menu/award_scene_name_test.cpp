#include "app/menu/award_scene_name.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

using sim::CircuitAward;

TEST_CASE("an award scene name names its award") {
    for (const CircuitAward award :
         {CircuitAward::Gold, CircuitAward::Silver, CircuitAward::Bronze, CircuitAward::None}) {
        CHECK(ParseAwardScene(AwardSceneName(award)) == award);
    }
}

TEST_CASE("only award names parse as award scenes") {
    CHECK(AwardSceneName(CircuitAward::Silver) == "menu:award:silver");
    CHECK(IsAwardScene("menu:award:none"));
    CHECK_FALSE(ParseAwardScene("menu:award:platinum").has_value());
    CHECK_FALSE(ParseAwardScene("menu:main").has_value());
}

}
}
