#include "sim/route_playback.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

assets::RoutePoint PointBy(float x, std::int32_t durationMs) {
    return {.offset = {.x = x}, .durationMs = durationMs};
}

assets::RaceRoute ThreePointRoute() {
    return {
        .startPosition = {.x = 10.0F},
        .loopPosition = {.x = 100.0F},
        .loopTimeMs = 100,
        .loopPoint = 0,
        .points = {PointBy(1.0F, 100), PointBy(2.0F, 40), PointBy(4.0F, 60)},
    };
}

TEST_CASE("route playback starts at the start pose") {
    const RoutePlayback playback(ThreePointRoute());

    CHECK(playback.Pose().position.x == 10.0F);
}

TEST_CASE("route playback blends within a segment") {
    RoutePlayback playback(ThreePointRoute());

    playback.Advance(Milliseconds{25});

    CHECK(playback.Pose().position.x == doctest::Approx(10.25F));
}

TEST_CASE("route playback adds up offsets across segments in one step") {
    RoutePlayback playback(ThreePointRoute());

    playback.Advance(Milliseconds{120});

    CHECK(playback.Pose().position.x == doctest::Approx(12.0F));
}

TEST_CASE("route playback jumps to the loop pose after the last point") {
    RoutePlayback playback(ThreePointRoute());

    playback.Advance(Milliseconds{200});
    CHECK(playback.Pose().position.x == 100.0F);

    playback.Advance(Milliseconds{20});
    CHECK(playback.Pose().position.x == doctest::Approx(101.0F));
}

TEST_CASE("route playback lands on the same pose in one step or many") {
    RoutePlayback once(ThreePointRoute());
    RoutePlayback often(ThreePointRoute());

    once.Advance(Milliseconds{456});
    for (int step = 0; step < 38; ++step) {
        often.Advance(Milliseconds{12});
    }

    CHECK(once.Pose() == often.Pose());
}

}
}
