#include "sim/race/respawn.hpp"

#include <doctest/doctest.h>

#include <numbers>
#include <optional>

#include "../physics/plane_world.hpp"
#include "checkpoint_ring.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr RespawnRule kOneSecond{.timeout = Milliseconds{1000}};
constexpr float kLowestTrackZ = -100.0F;

int TicksUntilRespawn(RespawnWatch& watch, const RespawnRule& rule, bool isGrounded, float z) {
    for (int tick = 1; tick <= 1000; ++tick) {
        if (watch.Update(rule, kTick, isGrounded, z, kLowestTrackZ)) {
            return tick;
        }
    }
    return 0;
}

TEST_CASE("a racer off the ground for the timeout respawns") {
    RespawnWatch watch;

    CHECK(TicksUntilRespawn(watch, kOneSecond, false, 0.0F) == 84);
}

TEST_CASE("touching the ground restarts the count") {
    RespawnWatch watch;
    for (int tick = 0; tick < 80; ++tick) {
        CHECK_FALSE(watch.Update(kOneSecond, kTick, false, 0.0F, kLowestTrackZ));
    }
    CHECK_FALSE(watch.Update(kOneSecond, kTick, true, 0.0F, kLowestTrackZ));

    CHECK(TicksUntilRespawn(watch, kOneSecond, false, 0.0F) == 84);
}

TEST_CASE("falling well below the floor respawns at once, even on the ground") {
    RespawnWatch watch;

    CHECK_FALSE(watch.Update(kOneSecond, kTick, true, kLowestTrackZ - 1.0F, kLowestTrackZ));
    CHECK(watch.Update(kOneSecond, kTick, true, kLowestTrackZ - 21.0F, kLowestTrackZ));
}

TEST_CASE("with the rule off, nothing respawns") {
    RespawnWatch watch;
    const RespawnRule off{.isEnabled = false};

    CHECK(TicksUntilRespawn(watch, off, false, kLowestTrackZ - 21.0F) == 0);
}

TEST_CASE("before any crossing, a racer respawns on its grid slot, dropped onto the track") {
    const CheckpointGraph graph = test::Ring();
    const test::PlaneWorld world = test::FlatGround();

    const RacerStart point = RespawnPoint(
        graph, std::nullopt, {.position = {.x = -20, .z = 8}, .headingRadians = 0.5F}, world);

    CHECK(point.position == Vector3{.x = -20});
    CHECK(point.headingRadians == 0.5F);
}

TEST_CASE("a racer respawns at its last checkpoint, facing through the next one") {
    const CheckpointGraph graph = test::Ring();
    const test::PlaneWorld world({test::TestPlane{.offset = -3.0F}});

    const RacerStart point = RespawnPoint(graph, 4, {}, world);

    CHECK(point.position == Vector3{.x = 20, .y = 500, .z = -3});
    CHECK(point.headingRadians == doctest::Approx(0.0F));
}

TEST_CASE("a checkpoint over a gap respawns the racer at the next one with track below") {
    const CheckpointGraph graph = test::Ring();
    const test::PlaneWorld world({test::TestPlane{.minX = 15.0F}});

    const RacerStart point = RespawnPoint(graph, 1, {}, world);

    CHECK(point.position == Vector3{.x = 20});
}

TEST_CASE("the respawn heading follows the next gate's travel direction") {
    const CheckpointGraph graph =
        CheckpointGraph::Create(
            {.checkpoints =
                 {
                     {.normal = {.y = 1.0F}, .links = {1, test::kNo, test::kNo, test::kNo}},
                     {.normal = {.y = -1.0F}, .links = {0, test::kNo, test::kNo, test::kNo}},
                 }})
            .value();
    const test::PlaneWorld world = test::FlatGround();

    CHECK(RespawnPoint(graph, 0, {}, world).headingRadians ==
          doctest::Approx(std::numbers::pi_v<float> / 2.0F));
    CHECK(RespawnPoint(graph, 1, {}, world).headingRadians ==
          doctest::Approx(-std::numbers::pi_v<float> / 2.0F));
}

}
}
