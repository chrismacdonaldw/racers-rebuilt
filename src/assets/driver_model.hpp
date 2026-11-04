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

/// Each named part takes its index from the catalogue. A part the catalogue lacks keeps its name
/// and index, and an index past its list becomes the first part.
[[nodiscard]] DriverLook FitLook(const DriverPartCatalog& catalog, DriverLook look);

/// Fits `look` and names each part that has no name yet.
[[nodiscard]] DriverLook NameLook(const DriverPartCatalog& catalog, const DriverLook& look);

/// True when `look` names a part the catalogue lacks, as a mod that's off leaves it.
[[nodiscard]] bool HasMissingParts(const DriverPartCatalog& catalog, const DriverLook& look);

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
