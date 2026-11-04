#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "assets/geometry_asset.hpp"
#include "assets/piece_library.hpp"
#include "assets/vfs.hpp"

namespace rr::assets {

/// A piece model's material with this name, in any case, takes the placed piece's colour.
inline constexpr std::string_view kPieceColourMaterial = "piece";

/// Keyed by upper-case piece name.
using PieceModels = std::unordered_map<std::string, GeometryAsset>;

/// The model there is in game units, laid on the piece's cells from the origin, bottom at z = 0.
[[nodiscard]] std::string PieceModelPath(std::string_view pieceName);

[[nodiscard]] std::optional<std::string> PieceOfModelPath(std::string_view path);

/// Chassis and the stud have none. A model that can't be read is logged and skipped.
[[nodiscard]] PieceModels ReadPieceModels(const Vfs& vfs, const PieceLibrary& library);

}
