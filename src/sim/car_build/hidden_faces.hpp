#pragma once

#include "sim/car_build/mesh_primitives.hpp"

namespace rr::sim {

/// Removes what flat lattice quads of different pieces share on a plane, keeping the rest.
void RemoveTouchingFaces(MeshPrimitives& primitives);

}
