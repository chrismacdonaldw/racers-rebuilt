#pragma once

#include <cstdint>
#include <string>

#include "assets/driver_look.hpp"
#include "assets/driver_parts.hpp"
#include "assets/geometry_asset.hpp"
#include "assets/part_library.hpp"
#include "core/result.hpp"

namespace rr::assets {

enum class DriverDetail : std::uint8_t { Menu, Race };

/// Menus pick the body by torso and legs variant. Races always use `leg_box`.
[[nodiscard]] Result<std::string> DriverBodyName(const DriverPartCatalog& catalog,
                                                 const DriverLook& look, DriverDetail detail);

/// Races show the default face, as the original builds them without the expression.
[[nodiscard]] Result<GeometryAsset> BuildDriverGeometry(const DriverPartCatalog& catalog,
                                                        const PartLibrary& heads,
                                                        const GeometryAsset& body,
                                                        const DriverLook& look,
                                                        DriverDetail detail);

}
