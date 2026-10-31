#include "sim/physics/body_effects.hpp"

#include <doctest/doctest.h>

#include <iterator>
#include <numeric>
#include <vector>

namespace rr::sim {
namespace {

constexpr int kTickMs = 12;
constexpr ChassisSpec kNeutralChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};

TEST_CASE("a missile spin turns once every 897 ms for 1,794 ms") {
    BodyState body;

    StartSpin(body, MakeBodyConstants(kNeutralChassis, {}), 2.0F, 0.007F, 0.0F);

    CHECK(body.spin.periodMs == 897);
    CHECK(body.impulses.yawMs == 1794);
}

TEST_CASE("a missile spin runs 152 steps of 12 ms but simulates 1,806 ms, cutting step 151") {
    BodyState body;
    StartSpin(body, MakeBodyConstants(kNeutralChassis, {}), 2.0F, 0.007F, 0.0F);

    std::vector<int> steps;
    while (body.spin.isSpinning) {
        steps.push_back(CountDownSpin(body, kTickMs));
    }

    std::vector<int> expected(152, kTickMs);
    expected.at(150) = 6;
    CHECK(steps == expected);
    CHECK(std::accumulate(steps.begin(), std::prev(steps.end()), 0) == 1806);
    CHECK(body.impulses.yawMs == 0);
}

}
}
