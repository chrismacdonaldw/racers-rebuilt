#pragma once

#include <optional>

#include "core/vector3.hpp"
#include "sim/physics/body_state.hpp"

namespace rr::sim {

struct ContactBox {
    Vector3 centre;
    Basis axes;
    Vector3 halfSize;
};

/// `normal` points from the second box to the first.
struct BoxContact {
    Vector3 normal;
    float penetration = 0.0F;
    Vector3 point;
};

[[nodiscard]] std::optional<BoxContact> TestBoxes(const ContactBox& first,
                                                  const ContactBox& second);

}
