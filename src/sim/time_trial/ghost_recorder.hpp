#pragma once

#include "assets/ghost_run.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"

namespace rr::sim {

/// Samples a car on its first tick, then on the first tick at or after each further 250 ms.
class GhostRecorder {
public:
    /// Call once per tick from the go, after the racers move. `pose` is in game space.
    void Record(Milliseconds elapsed, const Transform& pose);

    [[nodiscard]] const assets::GhostRun& Run() const { return run_; }

    /// Ends the run with `laps` and starts an empty one.
    [[nodiscard]] assets::GhostRun Take(const assets::GhostLapTimes& laps);

private:
    assets::GhostRun run_;
    Milliseconds untilSample_{0};
};

}
