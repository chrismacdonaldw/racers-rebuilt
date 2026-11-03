#include "sim/powerup/aim_targets.hpp"

#include <algorithm>

namespace rr::sim {

void AimTargets::Hear(std::span<const TrackEvent> events) {
    for (const TrackEvent& event : events) {
        if (event.edge != EventEdge::Start || event.racer || event.isPaired) {
            continue;
        }
        std::erase_if(left_, [&](const assets::TargetPoint& point) {
            return point.removingEvent == event.id;
        });
    }
}

}
