#pragma once

#include <cstdint>
#include <optional>

#include "core/vector3.hpp"

namespace rr::sim {

/// A trap's model position in game space, or the racer that passed the curse on.
struct CurseSource {
    std::optional<Vector3> trap;
    std::optional<std::uint8_t> giver;

    friend bool operator==(const CurseSource&, const CurseSource&) = default;
};

}
