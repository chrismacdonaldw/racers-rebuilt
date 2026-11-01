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

    /// A queued part waits for the current part to reach its last frame, looping or not.
    void Tick(Milliseconds elapsed);

    /// Does nothing to an entity without a clock.
    void Seek(std::size_t entity, float frame);

    /// Replaces any part already queued. An entity without a clock starts `part` at once.
    void Queue(std::size_t entity, std::size_t part, bool loops);

    [[nodiscard]] std::optional<std::size_t> CurrentPart(std::size_t entity) const;
    [[nodiscard]] bool HasQueued(std::size_t entity) const { return queued_[entity].has_value(); }

    [[nodiscard]] std::optional<std::size_t> Find(std::string_view name) const;

    [[nodiscard]] std::span<const EntityClock> Clocks() const { return clocks_; }
    [[nodiscard]] std::span<const EntityClock> Queued() const { return queued_; }

    /// 0 for an entity without a clock.
    [[nodiscard]] float Frame(std::size_t entity) const;

    /// In game space.
    [[nodiscard]] Transform NodeWorld(std::size_t entity, std::size_t node) const;

private:
    const TrackEntitySources* sources_;
    std::vector<EntityClock> clocks_;
    std::vector<EntityClock> queued_;
};

}
