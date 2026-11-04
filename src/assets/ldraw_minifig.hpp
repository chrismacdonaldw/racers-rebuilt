#pragma once

#include <array>
#include <span>
#include <string_view>

#include "assets/ldraw.hpp"

namespace rr::assets {

inline constexpr std::string_view kRootJoint = "*";
inline constexpr std::string_view kMinifigHeadFile = "3626b.dat";

/// In LDraw units from the torso top. `joints` texture it, `segments` (else `joints`) move it.
struct MinifigPiece {
    std::string_view file;
    LdrawMatrix placement;
    Vector3 pivot;
    std::string_view material;
    std::array<std::string_view, 4> joints;
    std::string_view swingJoint;
    std::array<std::string_view, 3> segments;
};

/// Matches the original body file name in any case. Empty for a body without an LDraw figure.
[[nodiscard]] std::span<const MinifigPiece> MinifigFigureOf(std::string_view bodyName);

}
