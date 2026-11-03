#pragma once

#include <span>

#include "assets/geometry_asset.hpp"
#include "assets/material_library.hpp"
#include "assets/track_loader.hpp"
#include "core/transform.hpp"

namespace rr::app {

/// `joints` place jointed vertices for lighting; empty lights them unplaced. Drops the normals.
void BakeBuiltRacerLight(assets::GeometryAsset& geometry,
                         const assets::NameMap<assets::MaterialAsset>& materials,
                         std::span<const Transform> joints);

}
