#pragma once

#include <cstddef>

#include "../../sim/race/checkpoint_ring.hpp"
#include "core/duration.hpp"
#include "sim/race/race_clock.hpp"
#include "sim/race/race_progress.hpp"

namespace rr::test {

inline constexpr Milliseconds kLapStep{100};
inline constexpr float kOffRouteY = 200.0F;

/// Racers on `Ring`, moved by hand rather than by physics.
struct RingRace {
    sim::CheckpointGraph graph = Ring();
    sim::RaceClock clock;
    sim::RaceProgress progress;

    RingRace(std::size_t racerCount, int lapCount) : progress(racerCount, lapCount) {}

    void Step(std::size_t racer, float fromX, float toX, float y = 0.0F) {
        progress.Move(
            graph, racer,
            {.from = {.x = fromX, .y = y}, .to = {.x = toX, .y = y}, .facing = {.x = 1.0F}},
            kLapStep, clock.IsRunning());
    }

    void Wait(std::size_t racer, Milliseconds elapsed) {
        progress.Move(graph, racer,
                      {.from = {.x = -5.0F}, .to = {.x = -5.0F}, .facing = {.x = 1.0F}}, elapsed,
                      clock.IsRunning());
    }

    /// Crosses every gate and returns to the start in 500 ms.
    void DriveLap(std::size_t racer) {
        for (const float x : {-5.0F, 5.0F, 15.0F, 25.0F}) {
            Step(racer, x, x + 10.0F);
        }
        Step(racer, 35.0F, -5.0F, kOffRouteY);
    }

    void StartRacing() {
        clock.Advance(Milliseconds{2001}, false);
        clock.Advance(Milliseconds{3000}, false);
        REQUIRE(clock.Phase() == sim::RacePhase::Racing);
    }
};

}
