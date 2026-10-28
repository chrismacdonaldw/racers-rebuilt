#include "sim/physics/drive_controller.hpp"

#include <doctest/doctest.h>

#include "sim/physics/car_body.hpp"

namespace rr::sim {
namespace {

TEST_CASE("full lock turns on a radius of 40 and a tiny input on about 4000") {
    CHECK(TurnRadiusFor(1.0F) == doctest::Approx(40.0F));
    CHECK(TurnRadiusFor(-1.0F) == doctest::Approx(-40.0F));
    CHECK(TurnRadiusFor(1.0e-6F) == doctest::Approx(4000.0F).epsilon(0.001));
    CHECK(TurnRadiusFor(0.0F) == 0.0F);
}

TEST_CASE("turn radii are clamped to 40 and drop to straight beyond 4096") {
    BodyState body;

    SetTurnRadius(body, -12.0F);
    CHECK(body.turnRadius == -40.0F);
    SetTurnRadius(body, 5000.0F);
    CHECK(body.turnRadius == 0.0F);
}

TEST_CASE("drive opposing the motion brakes at twice the thrust") {
    const BodyConstants constants = MakeBodyConstants({}, {});
    BodyState body = MakeBody(constants, {}, {});
    body.derived.forwardSpeed = 0.05F;

    ApplyDriveCommand(body, constants, {.drive = -1.0F}, {});

    CHECK(body.thrust == -108.0F);
}

TEST_CASE("a slow car steers on a fifth of the radius") {
    const BodyConstants constants = MakeBodyConstants({}, {});
    BodyState body = MakeBody(constants, {}, {});
    body.motion.contactCount = 4;
    for (WheelProbe& wheel : body.motion.wheels) {
        wheel.isInContact = true;
    }

    ApplyDriveCommand(body, constants, {.steering = 0.05F}, {});

    CHECK(body.turnRadius == doctest::Approx(0.2F * TurnRadiusFor(0.05F)));
    CHECK_FALSE(body.steering.isSteering);
}

}
}
