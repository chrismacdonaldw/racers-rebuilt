#include "sim/track/racer_tints.hpp"

namespace rr::sim {

RacerTints::RacerTints(const assets::EventTableAsset& table, std::size_t racerCount)
    : table_(&table), tints_(racerCount) {}

void RacerTints::Apply(std::span<const TrackEvent> events) {
    for (const TrackEvent& event : events) {
        if (event.racer && *event.racer < tints_.size()) {
            Apply(event, *event.racer);
        }
    }
}

void RacerTints::Apply(const TrackEvent& event, std::size_t racer) {
    std::optional<std::size_t>& tint = tints_[racer];
    for (std::size_t i = 0; i < table_->colorTransforms.size(); ++i) {
        const assets::ColorTransformResource& transform = table_->colorTransforms[i];
        const assets::EventTrigger& trigger = transform.trigger;
        if (trigger.event != event.id) {
            continue;
        }
        const bool isStart = event.edge == EventEdge::Start;
        if (isStart != trigger.startsOnEnd) {
            tint = transform.clears ? std::nullopt : std::optional(i);
        }
        if (!isStart && !trigger.ignoresEnd && !transform.clears) {
            tint.reset();
        }
    }
}

}
