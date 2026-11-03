#pragma once

#include <cstddef>
#include <vector>

#include "app/hud/minimap.hpp"
#include "core/vector3.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// Refills `markers` with every other racer, then the ghosts. `viewDirection` is in game space.
[[nodiscard]] MapView PlayerMapView(const sim::RenderSnapshot& snapshot, std::size_t own,
                                    std::size_t players, const Vector3& viewDirection,
                                    std::vector<MapMarker>& markers);

}
