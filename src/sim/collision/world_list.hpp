#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "core/vector3.hpp"
#include "sim/collision/collision_world.hpp"
#include "sim/physics/collision_query.hpp"

namespace rr::sim {

inline constexpr std::size_t kMaxRacerWorlds = 5;

/// Slot 0 is the track, tested last. The others are tested first, in slot order. Worlds must
/// outlive the list.
class WorldList final : public CollisionQuery {
public:
    explicit WorldList(const CollisionWorld& track);

    /// Does nothing if the world is already listed. Returns false when every slot is taken.
    bool Add(const CollisionWorld& world);
    void Remove(const CollisionWorld& world);

    [[nodiscard]] std::size_t WorldCount() const override { return count_; }
    [[nodiscard]] std::optional<SegmentHit> CastSegment(std::size_t world, const Vector3& from,
                                                        const Vector3& to) const override;

private:
    std::array<const CollisionWorld*, kMaxRacerWorlds> slots_{};
    std::size_t count_ = 1;
};

}
