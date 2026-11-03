#include "sim/powerup/aim_targets.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <vector>

#include "sim/race/random.hpp"
#include "sim/track/track_events.hpp"

namespace rr::sim {
namespace {

constexpr assets::EventId kRemoving = 7;
constexpr std::size_t kCapacity = 4;

std::vector<assets::TargetPoint> Points() {
    return {{.position = {.x = 1.0F}, .removingEvent = kRemoving},
            {.position = {.x = 2.0F}, .removingEvent = std::nullopt},
            {.position = {.x = 3.0F}, .removingEvent = kRemoving + 1},
            {.position = {.x = 4.0F}, .removingEvent = kRemoving}};
}

std::vector<float> LeftXs(const AimTargets& targets) {
    std::vector<float> xs;
    for (const assets::TargetPoint& point : targets.Left()) {
        xs.push_back(point.position.x);
    }
    return xs;
}

TEST_CASE("a position-path start removes every target point that names it") {
    AimTargets targets(Points());

    targets.Hear(std::vector<TrackEvent>{{.id = kRemoving, .edge = EventEdge::Start}});

    CHECK(LeftXs(targets) == std::vector{2.0F, 3.0F});
}

TEST_CASE("ends, racer-path starts and paired starts leave the target points") {
    AimTargets targets(Points());

    targets.Hear(
        std::vector<TrackEvent>{{.id = kRemoving, .edge = EventEdge::End},
                                {.id = kRemoving, .edge = EventEdge::Start, .racer = 0},
                                {.id = kRemoving, .edge = EventEdge::Start, .isPaired = true}});

    CHECK(LeftXs(targets) == std::vector{1.0F, 2.0F, 3.0F, 4.0F});
}

TEST_CASE("a combined fire is paired, so a hazard's event leaves the points a shot would take") {
    Random random{1};
    TrackEvents events(TrackEventSources{}, 0, false, random, kCapacity);
    AimTargets targets(Points());

    events.FireStartAndEnd(kRemoving, std::nullopt);
    targets.Hear(events.Events());
    const std::vector<float> afterCombined = LeftXs(targets);
    events.Fire({.id = kRemoving, .edge = EventEdge::Start});
    targets.Hear(events.Events());

    CHECK(afterCombined == std::vector{1.0F, 2.0F, 3.0F, 4.0F});
    CHECK(LeftXs(targets) == std::vector{2.0F, 3.0F});
}

}
}
