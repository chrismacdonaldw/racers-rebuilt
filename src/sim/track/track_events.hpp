#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "assets/track_triggers.hpp"
#include "core/duration.hpp"
#include "sim/race/random.hpp"
#include "sim/track/track_event.hpp"
#include "sim/track/track_timers.hpp"
#include "sim/track/trigger_lists.hpp"

namespace rr::sim {

struct TrackEventSources {
    std::vector<assets::TriggerSphere> racerSpheres;
    std::vector<assets::TriggerSphere> environmentSpheres;
    std::vector<assets::TrackTimer> timers;
};

class TrackEvents {
public:
    TrackEvents(const TrackEventSources& sources, std::size_t racerCount, bool isMirrored,
                Random& random);

    /// Clears the last tick's events and ends those of the spheres left behind.
    void BeginTick();

    /// Tests the spheres against where the racers ended up, then runs the timers.
    void Tick(Milliseconds elapsed, std::span<const TriggerRacer> racers, Random& random);

    [[nodiscard]] std::span<const TrackEvent> Events() const { return queue_.Events(); }
    [[nodiscard]] const RacerTriggers& RacerSpheres() const { return racerSpheres_; }
    [[nodiscard]] const EnvironmentTriggers& EnvironmentSpheres() const {
        return environmentSpheres_;
    }
    [[nodiscard]] const TrackTimers& Timers() const { return timers_; }

private:
    RacerTriggers racerSpheres_;
    EnvironmentTriggers environmentSpheres_;
    TrackTimers timers_;
    TrackEventQueue queue_;
};

}
