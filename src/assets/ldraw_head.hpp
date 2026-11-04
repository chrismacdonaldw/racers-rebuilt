#pragma once

#include "assets/geometry_asset.hpp"
#include "assets/image_asset.hpp"

namespace rr::assets {

/// Unrolls `head`'s print into the face image layout. Colour 16 and unknown ones are yellow.
[[nodiscard]] ImageAsset DrawHeadPrint(const GeometryAsset& head);

}
