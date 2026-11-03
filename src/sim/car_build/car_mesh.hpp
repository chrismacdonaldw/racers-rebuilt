#pragma once

#include <cstddef>
#include <cstdint>

#include "assets/geometry_asset.hpp"
#include "assets/piece_catalog.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "sim/car_build/built_car.hpp"

namespace rr::sim {

/// What tops the bricks. Each step down is the original's fallback when the mesh overflows.
enum class StudStyle : std::uint8_t { Studs, NubTexture, Plain };

/// `drawnPieces` falls short of the car's only when even plain bricks overflow.
struct CarMesh {
    assets::GeometryAsset geometry;
    StudStyle studs = StudStyle::Studs;
    std::size_t drawnPieces = 0;
};

/// In game units, centred on the chassis offset. Pieces are looked up in `catalog` by type.
[[nodiscard]] Result<CarMesh> BuildCarMesh(const BuiltCar& car,
                                           const assets::PieceCatalog& catalog);

/// Where `BuildCarMesh` puts the grid's corner, in game units.
[[nodiscard]] Result<Vector3> CarMeshOffset(const BuiltCar& car,
                                            const assets::PieceCatalog& catalog);

/// One studded piece in game units, its footprint's corner at the origin.
[[nodiscard]] Result<assets::GeometryAsset> BuildPieceMesh(const assets::PieceRecord& piece,
                                                           std::uint8_t colour, unsigned rotation,
                                                           const assets::PieceCatalog& catalog);

}
