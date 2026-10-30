#pragma once

#include <optional>

#include "core/duration.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/racer.hpp"
#include "sim/race/checkpoint_graph.hpp"

namespace rr::sim {

struct RespawnRule {
    bool isEnabled = true;
    Milliseconds timeout{3000};

    friend bool operator==(const RespawnRule&, const RespawnRule&) = default;
};

class RespawnWatch {
public:
    /// True once off the ground for the timeout or well below the track. The count then restarts.
    [[nodiscard]] bool Update(const RespawnRule& rule, Milliseconds elapsed, bool isGrounded,
                              float z, float lowestTrackZ);

private:
    Milliseconds airborne_{0};
};

/// On the track below the first checkpoint from the last crossed that has track below, facing on.
[[nodiscard]] RacerStart RespawnPoint(const CheckpointGraph& graph,
                                      std::optional<std::size_t> lastCheckpoint,
                                      const RacerStart& gridSlot, const CollisionQuery& world);

}
