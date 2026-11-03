#pragma once

#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// From a model's game-space X and Z axes, looked up in the original's 1024-step arccos table.
[[nodiscard]] render::UvOffset EnvironmentOffset(const Vector3& right, const Vector3& up);

}
