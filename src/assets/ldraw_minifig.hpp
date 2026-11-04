#pragma once

#include <array>
#include <span>
#include <string_view>

#include "assets/ldraw.hpp"

namespace rr::assets {

inline constexpr std::string_view kRootJoint = "*";
inline constexpr std::string_view kMinifigHeadFile = "3626b.dat";

/// `placement` is in LDraw units with the torso's top at the origin. `joints` prefix the body
/// joints that texture the piece; it moves with `swingJoint`, or else the first of `joints` the
/// body has.
struct MinifigPiece {
    std::string_view file;
    LdrawMatrix placement;
    std::string_view material;
    std::array<std::string_view, 4> joints;
    std::string_view swingJoint;
};

/// Matches the original body file name in any case. Empty for a body without an LDraw figure.
[[nodiscard]] std::span<const MinifigPiece> MinifigFigureOf(std::string_view bodyName);

}
