#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "assets/geometry_asset.hpp"
#include "assets/ldraw.hpp"
#include "assets/ldraw_part.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// `matrix` moves the loaded part onto the original piece's cells, with its bottom at z = 0.
struct LdrawPiece {
    std::string_view piece;
    std::string_view file;
    LdrawMatrix matrix;
};

[[nodiscard]] std::span<const LdrawPiece> LdrawPieces();

/// Matches `pieceName` in any case.
[[nodiscard]] std::optional<LdrawPiece> LdrawPieceFor(std::string_view pieceName);

/// A piece model: colour 16 becomes `kPieceColourMaterial`, others the nearest game colour.
[[nodiscard]] Result<GeometryAsset> LoadLdrawPiece(LdrawLibrary& library, const LdrawPiece& piece);

}
