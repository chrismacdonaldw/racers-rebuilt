#pragma once

#include <span>
#include <string_view>

#include "assets/geometry_asset.hpp"
#include "assets/ldraw.hpp"
#include "assets/ldraw_part.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// `matrix` puts it on the piece's cells; an empty `colour` leaves its main colour to the piece.
struct LdrawPart {
    std::string_view piece;
    std::string_view file;
    LdrawMatrix matrix;
    std::string_view colour;
};

/// Each piece's parts are next to each other.
[[nodiscard]] std::span<const LdrawPart> LdrawParts();

/// Matches `pieceName` in any case. Empty when the piece has no LDraw parts.
[[nodiscard]] std::span<const LdrawPart> LdrawPartsOf(std::string_view pieceName);

/// A main colour left to the piece becomes `kPieceColourMaterial`, others the nearest game colour.
[[nodiscard]] Result<GeometryAsset> LoadLdrawPiece(LdrawLibrary& library,
                                                   std::span<const LdrawPart> parts);

}
