#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/duration.hpp"
#include "core/transform.hpp"
#include "sim/animation/part_clock.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::sim {

using EntityClock = std::optional<PartClock>;

/// In game space. An entity without a clock stands in its rest pose.
[[nodiscard]] Transform NodeWorld(const TrackEntitySources& sources, std::size_t entity,
                                  const EntityClock& clock, std::size_t node);

/// `sources` must outlive it.
class TrackEntities {
public:
    explicit TrackEntities(const TrackEntitySources& sources);

    void Tick(Milliseconds elapsed);

    [[nodiscard]] std::optional<std::size_t> Find(std::string_view name) const;

    [[nodiscard]] std::span<const EntityClock> Clocks() const { return clocks_; }

    /// In game space.
    [[nodiscard]] Transform NodeWorld(std::size_t entity, std::size_t node) const;

private:
    const TrackEntitySources* sources_;
    std::vector<EntityClock> clocks_;
};

}
