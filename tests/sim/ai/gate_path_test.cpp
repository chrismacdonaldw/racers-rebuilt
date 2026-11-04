#include "sim/ai/gate_path.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../physics/plane_world.hpp"

namespace rr::sim {
namespace {

constexpr float kSide = 200.0F;
constexpr float kGateHeight = 5.0F;
constexpr float kRideHeight = 0.2F;
constexpr float kReach = 30.0F;
constexpr std::uint8_t kNo = assets::kNoLink;

assets::Checkpoint GateAt(float x, float y, std::uint8_t next) {
    return {.position = {.x = x, .y = y, .z = kGateHeight}, .links = {next, kNo, kNo, kNo}};
}

std::vector<assets::Checkpoint> Square() {
    return {GateAt(0.0F, 0.0F, 1), GateAt(kSide, 0.0F, 2), GateAt(kSide, kSide, 3),
            GateAt(0.0F, kSide, 0)};
}

GateLoop Traced(const CollisionQuery& world) {
    std::optional<GateLoop> loop = GateLoop::Trace(Square(), world);
    if (!loop) {
        throw std::runtime_error("the square traces no loop");
    }
    return std::move(*loop);
}

float FlatDistance(const Vector3& a, const Vector3& b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

float NearestTo(const GateLoop& loop, const Vector3& at) {
    float nearest = kSide;
    for (const GatePoint& point : loop.Points()) {
        nearest = std::min(nearest, FlatDistance(point.position, at));
    }
    return nearest;
}

bool IsOpenGround(const GatePoint& point) {
    return !point.isAirborne && std::abs(point.position.z - kRideHeight) < 1e-4F &&
           std::abs(point.widthLeft - kReach) < 1e-4F &&
           std::abs(point.widthRight - kReach) < 1e-4F;
}

std::vector<Vector3> Positions(const assets::RaceRoute& route) {
    std::vector<Vector3> positions;
    Vector3 position = route.startPosition;
    for (const assets::RoutePoint& point : route.points) {
        position += point.offset;
        positions.push_back(position);
    }
    return positions;
}

std::int32_t TimeTo(std::span<const assets::RoutePoint> points, std::size_t last) {
    std::int32_t timeMs = 0;
    for (std::size_t i = 0; i <= last; ++i) {
        timeMs += points[i].durationMs;
    }
    return timeMs;
}

TEST_CASE("a gate loop rides the floor past every gate on open ground") {
    const test::PlaneWorld world = test::FlatGround();

    const GateLoop loop = Traced(world);

    CHECK(loop.Points().size() > 40);
    CHECK(std::ranges::all_of(loop.Points(), IsOpenGround));
    float farthest = 0.0F;
    for (const assets::Checkpoint& gate : Square()) {
        farthest = std::max(farthest, NearestTo(loop, gate.position));
    }
    CHECK(farthest < 40.0F);
}

TEST_CASE("a gate loop moves away from a wall beside its gates") {
    constexpr float kWallGap = 10.0F;
    const test::PlaneWorld world(
        {test::TestPlane{}, test::TestPlane{.normal = {.y = 1.0F}, .offset = -kWallGap}});

    const GateLoop loop = Traced(world);

    const auto beside = std::ranges::find_if(loop.Points(), [](const GatePoint& point) {
        return point.position.y < kSide / 2.0F &&
               std::abs(point.position.x - (kSide / 2.0F)) < 6.0F;
    });
    REQUIRE(beside != loop.Points().end());
    CHECK(beside->position.y > kWallGap);
    CHECK(beside->widthRight > kWallGap);
}

TEST_CASE("a gate loop marks where the floor is missing as airborne") {
    constexpr float kGapStart = 80.0F;
    constexpr float kGapEnd = 120.0F;
    const test::PlaneWorld world(
        {test::TestPlane{.maxX = kGapStart}, test::TestPlane{.minX = kGapEnd}});

    const GateLoop loop = Traced(world);

    const auto airborne = std::ranges::count_if(loop.Points(), &GatePoint::isAirborne);
    const auto strays = std::ranges::count_if(loop.Points(), [](const GatePoint& point) {
        return point.isAirborne && (point.position.x < kGapStart || point.position.x > kGapEnd);
    });
    CHECK(airborne > 0);
    CHECK(strays == 0);
}

TEST_CASE("a gate route drives from the grid onto the loop and laps it") {
    const test::PlaneWorld world = test::FlatGround();
    const GateLoop loop = Traced(world);
    const RacerStart start{.position = {.x = -40.0F, .z = kRideHeight}};

    const assets::RaceRoute route = loop.RouteFrom(start, world);

    REQUIRE(route.loopPoint + loop.Points().size() + 1 == route.points.size());
    const std::vector<Vector3> positions = Positions(route);
    CHECK(FlatDistance(positions.front(), start.position) < 15.0F);
    CHECK(Length(route.loopPosition - positions[route.loopPoint]) < 0.01F);
    CHECK(Length(positions.back() - positions[route.loopPoint]) < 0.01F);
}

TEST_CASE("a gate route times every point and reaches its loop at the loop time") {
    const test::PlaneWorld world = test::FlatGround();

    const assets::RaceRoute route =
        Traced(world).RouteFrom({.position = {.x = -40.0F, .z = kRideHeight}}, world);

    CHECK(route.loopTimeMs == TimeTo(route.points, route.loopPoint));
    CHECK(std::ranges::all_of(route.points, [](const assets::RoutePoint& point) {
        return point.durationMs > 0 && point.kind == 3;
    }));
}

TEST_CASE("a checkpoint too far away or not a number traces no loop") {
    const test::PlaneWorld world = test::FlatGround();
    std::vector<assets::Checkpoint> far = Square();
    far[2].position.x = 1.0e6F;
    std::vector<assets::Checkpoint> broken = Square();
    broken[1].position.z = std::numeric_limits<float>::quiet_NaN();

    CHECK_FALSE(GateLoop::Trace(far, world).has_value());
    CHECK_FALSE(GateLoop::Trace(broken, world).has_value());
}

TEST_CASE("checkpoints that never lead back to the first trace no loop") {
    const test::PlaneWorld world = test::FlatGround();
    const std::array open{GateAt(0.0F, 0.0F, 1), GateAt(kSide, 0.0F, kNo)};

    CHECK_FALSE(GateLoop::Trace(open, world).has_value());
    CHECK_FALSE(GateLoop::Trace({}, world).has_value());
}

}
}
