#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "assets/event_table.hpp"
#include "sim/track/track_event.hpp"

namespace rr::sim {

/// The color transform each racer's car takes from racer-path events. `table` must outlive it.
class RacerTints {
public:
    RacerTints(const assets::EventTableAsset& table, std::size_t racerCount);

    void Apply(std::span<const TrackEvent> events);

    /// Indexes the table's color transforms.
    [[nodiscard]] std::optional<std::size_t> Of(std::size_t racer) const {
        return tints_.at(racer);
    }

    [[nodiscard]] std::span<const std::optional<std::size_t>> All() const { return tints_; }

private:
    void Apply(const TrackEvent& event, std::size_t racer);

    const assets::EventTableAsset* table_;
    std::vector<std::optional<std::size_t>> tints_;
};

}
