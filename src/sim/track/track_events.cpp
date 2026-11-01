#include "sim/track/track_events.hpp"

namespace rr::sim {

namespace {

std::size_t MostEventsPerTick(const TrackEventSources& sources, std::size_t racerCount) {
    return (sources.racerSpheres.size() * (racerCount + 1) * 2) +
           (sources.environmentSpheres.size() * 2) + sources.timers.size();
}

}

TrackEvents::TrackEvents(const TrackEventSources& sources, std::size_t racerCount, bool isMirrored,
                         Random& random, std::size_t mostFiredPerTick)
    : racerSpheres_(sources.racerSpheres, isMirrored),
      environmentSpheres_(sources.environmentSpheres, isMirrored),
      timers_(sources.timers, random),
      queue_(MostEventsPerTick(sources, racerCount) + mostFiredPerTick) {}

void TrackEvents::BeginTick() {
    queue_.Clear();
    racerSpheres_.EndFrame(queue_);
    environmentSpheres_.EndFrame(queue_);
}

void TrackEvents::FireStartAndEnd(assets::EventId id, std::optional<Vector3> position) {
    queue_.Fire({.id = id, .edge = EventEdge::Start, .position = position});
    queue_.Fire({.id = id, .edge = EventEdge::End, .position = position});
}

void TrackEvents::TestSpheres(std::span<const TriggerRacer> racers) {
    racerSpheres_.Test(racers, queue_);
    environmentSpheres_.Test(racers, queue_);
}

void TrackEvents::TickTimers(Milliseconds elapsed, Random& random) {
    timers_.Tick(elapsed, random, queue_);
}

}
