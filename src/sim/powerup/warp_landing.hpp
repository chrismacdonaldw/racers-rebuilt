#pragma once

#include <cstddef>

#include "core/vector3.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/race/checkpoint_graph.hpp"

namespace rr::sim {

/// A flat heading from `ground` along the gate walk from `gate`, turned off any wall ahead.
[[nodiscard]] Vector3 ClearLandingHeading(const CollisionQuery& world,
                                          const CheckpointGraph& checkpoints, const Vector3& ground,
                                          std::size_t gate);

}
