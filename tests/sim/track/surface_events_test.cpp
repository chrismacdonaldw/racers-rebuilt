#include "sim/track/surface_events.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <iterator>
#include <vector>

#include "../physics/plane_world.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
constexpr RacerControls kThrottle{.isThrottleHeld = true};
constexpr assets::EventId kEnter = 40;
constexpr assets::EventId kLeave = 41;
constexpr assets::EventId kTouch = 101;
constexpr float kMudEnd = 5.0F;
constexpr float kGateX = 30.0F;
constexpr float kPastGate = 10.0F;
constexpr int kMostTicks = 1000;

std::vector<assets::SurfaceEntry> Entries() {
    return {{.name = "mud", .enterEvent = kEnter, .leaveEvent = kLeave}};
}

test::PlaneWorld MudThenGate() {
    return test::PlaneWorld({test::TestPlane{.surface = {.tableEntry = 0}, .maxX = kMudEnd},
                             test::TestPlane{.minX = kMudEnd},
                             test::TestPlane{.normal = {.x = -1.0F},
                                             .offset = -kGateX,
                                             .surface = {.touchEvent = kTouch},
                                             .isSolid = false}});
}

std::vector<TrackEvent> DriveThrough() {
    const test::PlaneWorld world = MudThenGate();
    const TrackSurfaces surfaces(Entries());
    Random random(1);
    TrackEvents events(TrackEventSources{}, 1, false, random, SurfaceEvents::MostEventsPerRacer());
    SurfaceEvents heard(1);
    Racer car(kChassis, {}, {.position = {.z = 0.2F}});
    car.Go();
    std::vector<TrackEvent> fired;
    for (int tick = 0; tick < kMostTicks && car.Body().motion.position.x < kGateX + kPastGate;
         ++tick) {
        events.BeginTick();
        car.Tick(kTick, kThrottle, world);
        car.UpdateTimers(kTick);
        heard.Hear(0, car, surfaces, events);
        std::ranges::copy(events.Events(), std::back_inserter(fired));
    }
    return fired;
}

std::vector<TrackEvent> Matching(const std::vector<TrackEvent>& events, assets::EventId id) {
    std::vector<TrackEvent> matching;
    std::ranges::copy_if(events, std::back_inserter(matching),
                         [&](const TrackEvent& event) { return event.id == id; });
    return matching;
}

TEST_CASE("wheels moving onto and off a surface fire its events on both paths") {
    const std::vector<TrackEvent> fired = DriveThrough();
    const std::vector<TrackEvent> entered = Matching(fired, kEnter);
    const std::vector<TrackEvent> left = Matching(fired, kLeave);

    REQUIRE(entered.size() == 2);
    CHECK(entered[0].edge == EventEdge::Start);
    CHECK(entered[0].position.has_value());
    CHECK(entered[1].racer == 0);
    REQUIRE(left.size() == 2);
    CHECK(left[0].edge == EventEdge::End);
    CHECK(left[0].position.has_value());
    CHECK(left[1].racer == 0);
}

TEST_CASE("a body passing a touch surface fires its start and end together on both paths") {
    const std::vector<TrackEvent> touched = Matching(DriveThrough(), kTouch);

    REQUIRE(touched.size() >= 4);
    CHECK(touched[0].edge == EventEdge::Start);
    CHECK(touched[0].position.value_or(Vector3{}).x == doctest::Approx(kGateX));
    CHECK(touched[1].racer == 0);
    CHECK(touched[2].edge == EventEdge::End);
    CHECK(touched[3].racer == 0);
}

}
}
