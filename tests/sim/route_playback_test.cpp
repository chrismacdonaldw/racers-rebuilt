#include "sim/route_playback.hpp"

#include <doctest/doctest.h>

#include <cstdint>

namespace rr::sim {
namespace {

assets::RoutePoint PointBy(float x, std::int32_t durationMs, std::uint8_t kind = 3) {
    return {.offset = {.x = x}, .kind = kind, .durationMs = durationMs};
}

assets::RaceRoute ThreePointRoute() {
    return {
        .startPosition = {.x = 10.0F},
        .loopPosition = {.x = 100.0F},
        .loopTimeMs = 100,
        .loopPoint = 0,
        .points = {PointBy(1.0F, 100, 2), PointBy(2.0F, 40, 0), PointBy(4.0F, 60)},
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

TEST_CASE("route playback scales time by its speed and keeps the fraction") {
    RoutePlayback playback(ThreePointRoute());

    playback.Advance(Milliseconds{25}, 0.5F);
    CHECK(playback.Pose().position.x == doctest::Approx(10.12F));

    playback.Advance(Milliseconds{25}, 0.5F);
    CHECK(playback.Pose().position.x == doctest::Approx(10.25F));
}

TEST_CASE("route playback runs backward across segments and stops at the start") {
    RoutePlayback playback(ThreePointRoute());
    playback.Advance(Milliseconds{130});
    CHECK(playback.Pose().position.x == doctest::Approx(12.5F));

    playback.Advance(Milliseconds{40}, -1.0F);
    CHECK(playback.Pose().position.x == doctest::Approx(10.9F));
    CHECK(playback.ContactCount() == 2);

    playback.Advance(Milliseconds{200}, -1.0F);
    CHECK(playback.Pose().position.x == 10.0F);
}

TEST_CASE("route playback reports the wheels down at the point it approaches, all at the start") {
    RoutePlayback playback(ThreePointRoute());
    CHECK(playback.ContactCount() == 4);

    playback.Advance(Milliseconds{100});
    CHECK(playback.ContactCount() == 0);

    playback.Advance(Milliseconds{40});
    CHECK(playback.ContactCount() == 4);
}

TEST_CASE("route playback seeks by a move's length along the segment, either way") {
    RoutePlayback playback(ThreePointRoute());

    playback.Seek({.x = 0.5F, .y = 3.0F});
    CHECK(playback.Pose().position.x == doctest::Approx(10.5F));

    playback.Seek({.x = -0.4F});
    CHECK(playback.Pose().position.x == doctest::Approx(10.1F));

    playback.Seek({.x = -5.0F});
    CHECK(playback.Pose().position.x == 10.0F);
}

}
}
