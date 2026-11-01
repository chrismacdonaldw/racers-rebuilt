#include "sim/track/track_events.hpp"

namespace rr::sim {

namespace {

std::size_t MostEventsPerTick(const TrackEventSources& sources, std::size_t racerCount) {
    return (sources.racerSpheres.size() * racerCount * 2) +
           (sources.environmentSpheres.size() * 2) + sources.timers.size();
}

}

TrackEvents::TrackEvents(const TrackEventSources& sources, std::size_t racerCount, bool isMirrored,
                         Random& random)
    : racerSpheres_(sources.racerSpheres, isMirrored),
      environmentSpheres_(sources.environmentSpheres, isMirrored),
      timers_(sources.timers, random),
      queue_(MostEventsPerTick(sources, racerCount)) {}

void TrackEvents::BeginTick() {
    queue_.Clear();
    racerSpheres_.EndFrame(queue_);
    environmentSpheres_.EndFrame(queue_);
}

void TrackEvents::Tick(Milliseconds elapsed, std::span<const TriggerRacer> racers, Random& random) {
    racerSpheres_.Test(racers, queue_);
    environmentSpheres_.Test(racers, queue_);
    timers_.Tick(elapsed, random, queue_);
}

}
