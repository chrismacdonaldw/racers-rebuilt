#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

/// `position` is in the bind pose, in model space and unscaled units.
struct RigPivot {
    std::size_t node = 0;
    Vector3 position;
};

/// Upper, middle and end nodes, each with a pivot.
struct RigLimb {
    std::array<std::size_t, 3> nodes{};
};

/// `bind` holds the unscaled rest models the figure was fitted to, when not its skeleton's own.
struct FigureRig {
    std::vector<RigPivot> pivots;
    std::vector<RigLimb> limbs;
    std::vector<Transform> bind;
};

[[nodiscard]] std::string WriteFigureRig(const FigureRig& rig);

/// Fails on a limb node without a pivot, or a limb segment too short to bend.
[[nodiscard]] Result<FigureRig> ParseFigureRig(std::span<const std::byte> bytes);

}
