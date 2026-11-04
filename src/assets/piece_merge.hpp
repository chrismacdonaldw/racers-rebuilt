#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "assets/piece_library.hpp"

namespace rr::assets {

inline constexpr std::uint16_t kFirstBrickType = 0x1389;

/// `materials` names the layer's MDB materials in the order its commands index them.
struct PieceLayer {
    PieceLibrary library;
    std::vector<std::string> materials;
};

/// Mod pieces replace same-named ones of their type or add unused brick types below the extra
/// pieces'. Others are left out as problems.
[[nodiscard]] PieceLayer MergePieceLayers(std::vector<PieceLayer> layers,
                                          std::vector<std::string>& problems);

}
