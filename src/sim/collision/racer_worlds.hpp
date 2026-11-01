#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>

#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "sim/collision/track_collision.hpp"
#include "sim/collision/track_surfaces.hpp"
#include "sim/physics/collision_query.hpp"

namespace rr::sim {

inline constexpr std::size_t kMaxTriggerWorlds = 3;
inline constexpr Milliseconds kTriggerWorldTime{150};

struct TriggerWorld {
    std::size_t world = 0;
    Milliseconds left{0};
};

class TriggerWorlds {
public:
    /// Does nothing when every slot is taken.
    void SwitchOn(std::size_t world);
    void Tick(Milliseconds elapsed);

    [[nodiscard]] std::span<const TriggerWorld> Worlds() const {
        return std::span(slots_).first(count_);
    }

private:
    std::array<TriggerWorld, kMaxTriggerWorlds> slots_{};
    std::size_t count_ = 0;
};

/// Tests checkpoint, trigger (at `moved` where set) and `base` worlds; surfaces set solidity.
class RacerWorldView final : public CollisionQuery {
public:
    RacerWorldView(const CollisionQuery& base, const TrackCollision* collision,
                   const TriggerWorlds& switched, const TrackSurfaces& surfaces,
                   std::span<const std::optional<Placement>> moved = {});

    [[nodiscard]] std::size_t WorldCount() const override { return count_ + base_->WorldCount(); }
    [[nodiscard]] std::optional<SegmentHit> CastSegment(std::size_t world, const Vector3& from,
                                                        const Vector3& to) const override;

private:
    const CollisionQuery* base_;
    const TrackSurfaces* surfaces_;
    std::array<const CollisionWorld*, kMaxTriggerWorlds + 1> extras_{};
    std::array<const Placement*, kMaxTriggerWorlds + 1> placements_{};
    std::size_t count_ = 0;
};

}
