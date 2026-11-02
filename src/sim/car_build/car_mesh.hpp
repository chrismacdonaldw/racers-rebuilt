#pragma once

#include <cstddef>
#include <cstdint>

#include "assets/geometry_asset.hpp"
#include "assets/piece_catalog.hpp"
#include "core/result.hpp"
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

}
