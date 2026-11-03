#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include "assets/brick_layout.hpp"
#include "core/vector3.hpp"
#include "sim/physics/body_state.hpp"

namespace rr::sim {

/// Red lines up the nearest rival ahead and yellow the nearest behind. Other colours aim nowhere.
[[nodiscard]] std::optional<Vector3> AimFor(std::optional<assets::BrickColour> colour,
                                            std::span<const Vector3> positions, std::size_t self,
                                            const BodyMotion& motion);

}
