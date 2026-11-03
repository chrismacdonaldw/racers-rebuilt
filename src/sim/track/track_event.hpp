#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "assets/event_id.hpp"
#include "core/assert.hpp"
#include "core/vector3.hpp"

namespace rr::sim {

enum class EventEdge : std::uint8_t { Start, End };

/// Racer-path events name their `racer`. A paired event was fired in one pass with an end.
struct TrackEvent {
    assets::EventId id = assets::kNoEvent;
    EventEdge edge = EventEdge::Start;
    std::optional<Vector3> position;
    std::optional<std::uint8_t> racer;
    bool isPaired = false;

    friend bool operator==(const TrackEvent&, const TrackEvent&) = default;
};

/// Drops events with no id.
class TrackEventQueue {
public:
    explicit TrackEventQueue(std::size_t capacity) { events_.reserve(capacity); }

    void Clear() { events_.clear(); }

    void Fire(const TrackEvent& event) {
        if (event.id != assets::kNoEvent) {
            RR_ASSERT(events_.size() < events_.capacity());
            events_.push_back(event);
        }
    }

    [[nodiscard]] std::span<const TrackEvent> Events() const { return events_; }

private:
    std::vector<TrackEvent> events_;
};

}
