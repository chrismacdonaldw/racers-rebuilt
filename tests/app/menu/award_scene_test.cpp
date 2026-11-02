#include "app/menu/award_scene.hpp"

#include <doctest/doctest.h>

namespace rr::app {

TEST_CASE("each award plays the original's tune for its place") {
    CHECK(AwardMusic(sim::CircuitAward::Gold) == "1st");
    CHECK(AwardMusic(sim::CircuitAward::Silver) == "2nd");
    CHECK(AwardMusic(sim::CircuitAward::Bronze) == "3rd");
    CHECK(AwardMusic(sim::CircuitAward::None) == "lose");
}

}
