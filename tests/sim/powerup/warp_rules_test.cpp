#include "sim/powerup/warp_rules.hpp"

#include <doctest/doctest.h>

namespace rr::sim {

TEST_CASE("the original warp walks at 0.6 units/ms, plays routes at 4.5 and launches") {
    CHECK(PaceFor({}, {}, false) == WarpPace{});
    CHECK(PaceFor({}, {}, true) == WarpPace{});
    CHECK(WarpPace{}.walkPerMs == 0.6F);
    CHECK(WarpPace{}.routeSpeed == 4.5F);
    CHECK(WarpPace{}.launch == WarpLaunch::Impulse);
}

TEST_CASE("a fair warp walks at three times top speed, plays routes at 3 and lands at top speed") {
    const WarpPace pace = PaceFor({.isFair = true}, {}, false);
    CHECK(pace.walkPerMs == doctest::Approx(0.36F));
    CHECK(pace.routeSpeed == doctest::Approx(3.0F));
    CHECK(pace.launch == WarpLaunch::TopSpeed);
}

TEST_CASE("a catch-up warp leaves the leader a third of the gain") {
    CHECK(PaceFor({.catchesUp = true}, {}, false) == WarpPace{});
    const WarpPace leader = PaceFor({.catchesUp = true}, {}, true);
    CHECK(leader.walkPerMs == doctest::Approx(0.28F));
    CHECK(leader.routeSpeed == doctest::Approx(1.0F + (3.5F / 3.0F)));
    CHECK(leader.launch == WarpLaunch::Impulse);

    const WarpPace fairLeader = PaceFor({.isFair = true, .catchesUp = true}, {}, true);
    CHECK(fairLeader.walkPerMs == doctest::Approx(0.2F));
    CHECK(fairLeader.routeSpeed == doctest::Approx(1.0F + (2.0F / 3.0F)));
}

}
