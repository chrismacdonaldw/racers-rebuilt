#pragma once

#include <span>

#include "assets/figure_rig.hpp"
#include "assets/geometry_asset.hpp"
#include "assets/ldraw_minifig.hpp"
#include "assets/part_library.hpp"
#include "assets/skeleton_database.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// `geometry` is the piece's LDraw part as `LdrawLibrary::LoadPart` gives it.
struct FigurePiece {
    const MinifigPiece* piece = nullptr;
    GeometryAsset geometry;
};

struct FittedMinifig {
    GeometryAsset geometry;
    FigureRig rig;
};

/// The first piece is the torso. Original `face` triangles stay so the head still finds its joint.
[[nodiscard]] Result<FittedMinifig> FitMinifigBody(const GeometryAsset& body,
                                                   const SkeletonAsset& skeleton,
                                                   std::span<const FigurePiece> pieces);

/// Swaps each part's `face` group for `head`, textured from that group.
[[nodiscard]] PartLibrary FitMinifigHeads(PartLibrary heads, const GeometryAsset& head);

}
