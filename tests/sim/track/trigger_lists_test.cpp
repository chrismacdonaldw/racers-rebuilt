#include "sim/track/trigger_lists.hpp"

#include <doctest/doctest.h>

#include <array>
#include <span>
#include <vector>

namespace rr::sim {
namespace {

constexpr assets::TriggerSphere kGate{.centre = {.x = 100.0F}, .radius = 10.0F, .event = 7};

TriggerRacer At(float x, bool isPlayer = true, int completedLaps = 0) {
    return {.reach = {.centre = {.x = x}, .radius = 2.0F},
            .isPlayer = isPlayer,
            .completedLaps = completedLaps};
}

template <typename Triggers>
std::vector<TrackEvent> Step(Triggers& triggers, std::span<const TriggerRacer> racers) {
    TrackEventQueue queue(triggers.Spheres().size() * kMaxTriggerRacers * 2);
    triggers.EndFrame(queue);
    triggers.Test(racers, queue);
    return {queue.Events().begin(), queue.Events().end()};
}

std::vector<TrackEvent> RacerStep(RacerTriggers& triggers, std::span<const TriggerRacer> racers) {
    std::vector<TrackEvent> events = Step(triggers, racers);
    std::erase_if(events, [](const TrackEvent& event) { return !event.racer; });
    return events;
}

TrackEvent RacerEvent(EventEdge edge, std::uint8_t racer, assets::EventId id = 7) {
    return {.id = id, .edge = edge, .racer = racer};
}

TEST_CASE("a racer starts a sphere's event once and ends it after a whole tick outside") {
    const std::array spheres{kGate};
    RacerTriggers triggers(spheres, false);
    const std::array inside{At(105.0F)};
    const std::array outside{At(200.0F)};

    CHECK(RacerStep(triggers, inside) == std::vector{RacerEvent(EventEdge::Start, 0)});
    CHECK(RacerStep(triggers, inside).empty());
    CHECK(RacerStep(triggers, outside).empty());
    CHECK(RacerStep(triggers, outside) == std::vector{RacerEvent(EventEdge::End, 0)});
    CHECK(RacerStep(triggers, outside).empty());
}

TEST_CASE("each racer fires a racer sphere for itself") {
    const std::array spheres{kGate};
    RacerTriggers triggers(spheres, false);

    CHECK(RacerStep(triggers, std::array{At(95.0F), At(0.0F)}) ==
          std::vector{RacerEvent(EventEdge::Start, 0)});
    CHECK(RacerStep(triggers, std::array{At(95.0F), At(108.0F)}) ==
          std::vector{RacerEvent(EventEdge::Start, 1)});
    CHECK((triggers.Inside(0) & 0b11U) == 0b11U);
}

TEST_CASE("a sphere touches a racer whose reach overlaps it") {
    const std::array spheres{kGate};
    RacerTriggers triggers(spheres, false);

    CHECK(RacerStep(triggers, std::array{At(112.0F)}).empty());
    CHECK(RacerStep(triggers, std::array{At(111.9F)}).size() == 1);
}

TEST_CASE("player-only and lap spheres ignore the racers they don't accept") {
    std::array spheres{kGate, kGate};
    spheres[0].isPlayerOnly = true;
    spheres[1].lap = 1;
    spheres[1].event = 8;
    RacerTriggers triggers(spheres, false);

    CHECK(RacerStep(triggers, std::array{At(100.0F, false, 0)}).empty());
    CHECK(RacerStep(triggers, std::array{At(100.0F, false, 1)}) ==
          std::vector{RacerEvent(EventEdge::Start, 0, 8)});
}

TEST_CASE("a projectile sphere and a sphere without an event fire nothing for racers") {
    std::array spheres{kGate, kGate};
    spheres[0].isForProjectiles = true;
    spheres[1].event = assets::kNoEvent;
    RacerTriggers triggers(spheres, false);

    CHECK(RacerStep(triggers, std::array{At(100.0F)}).empty());
    CHECK(triggers.Inside(0) == 0);
    CHECK(triggers.Inside(1) == 1);
}

TEST_CASE("a mirrored track flips the spheres across the x axis") {
    const std::array spheres{
        assets::TriggerSphere{.centre = {.y = 50.0F}, .radius = 5.0F, .event = 3}};
    RacerTriggers triggers(spheres, true);

    CHECK(triggers.Spheres().front().centre == Vector3{.y = -50.0F});
    CHECK(
        RacerStep(triggers, std::array{TriggerRacer{.reach = {.centre = {.y = -50.0F}}}}).size() ==
        1);
}

TEST_CASE("a racer sphere also fires at its centre while anyone is inside") {
    const std::array spheres{kGate};
    RacerTriggers triggers(spheres, false);
    const TrackEvent start{.id = 7, .edge = EventEdge::Start, .position = kGate.centre};
    const TrackEvent end{.id = 7, .edge = EventEdge::End, .position = kGate.centre};
    const std::array both{At(105.0F), At(95.0F)};
    const std::array outside{At(200.0F)};

    CHECK(Step(triggers, both) ==
          std::vector{start, RacerEvent(EventEdge::Start, 0), RacerEvent(EventEdge::Start, 1)});
    CHECK(triggers.IsActive(0));
    CHECK(Step(triggers, outside).empty());
    CHECK(Step(triggers, outside) ==
          std::vector{end, RacerEvent(EventEdge::End, 0), RacerEvent(EventEdge::End, 1)});
    CHECK_FALSE(triggers.IsActive(0));
}

TEST_CASE("an environment sphere fires at its centre while anyone is inside") {
    const std::array spheres{kGate};
    EnvironmentTriggers triggers(spheres, false);
    const TrackEvent start{.id = 7, .edge = EventEdge::Start, .position = kGate.centre};
    const TrackEvent end{.id = 7, .edge = EventEdge::End, .position = kGate.centre};

    CHECK(Step(triggers, std::array{At(100.0F), At(0.0F)}) == std::vector{start});
    CHECK(Step(triggers, std::array{At(0.0F), At(100.0F)}).empty());
    CHECK(triggers.IsActive(0));
    CHECK(Step(triggers, std::array{At(0.0F), At(0.0F)}).empty());
    CHECK(Step(triggers, std::array{At(0.0F), At(0.0F)}) == std::vector{end});
    CHECK_FALSE(triggers.IsActive(0));
}

TEST_CASE("a player-only environment sphere ignores computer racers") {
    std::array spheres{kGate};
    spheres[0].isPlayerOnly = true;
    EnvironmentTriggers triggers(spheres, false);

    CHECK(Step(triggers, std::array{At(100.0F, false)}).empty());
    CHECK_FALSE(triggers.IsActive(0));
}

}
}
