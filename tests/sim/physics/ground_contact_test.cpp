#include "sim/physics/ground_contact.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

const Surface kMud{.rollingResistance = 0.4F,
                   .supportThreshold = 0.2F,
                   .friction = 0.1F,
                   .lateralGrip = 1.0F,
                   .force = {.x = 2.0F},
                   .soundId = 7,
                   .touchEvent = 3,
                   .tableEntry = 5};

BodyMotion OnMud() {
    BodyMotion motion;
    for (WheelProbe& wheel : motion.wheels) {
        wheel.isInContact = true;
        wheel.hitSurface = kMud;
    }
    motion.wheels[3].isInContact = false;
    return motion;
}

TEST_CASE("a wheel takes the surface under it, and the road once off the ground") {
    BodyMotion motion = OnMud();
    UpdateWheelSurfaces(motion, false);
    CHECK(motion.wheels[0].surface == kMud);
    CHECK(motion.wheels[3].surface == Surface{});
}

TEST_CASE("ignoring surfaces drives on road handling but keeps the real sound and events") {
    BodyMotion motion = OnMud();
    UpdateWheelSurfaces(motion, true);
    const Surface ignored{.soundId = 7, .touchEvent = 3, .tableEntry = 5};
    CHECK(motion.wheels[0].surface == ignored);
    CHECK(motion.wheels[3].surface == Surface{});
}

}
}
