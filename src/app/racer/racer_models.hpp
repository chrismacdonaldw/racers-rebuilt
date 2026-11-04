#pragma once

#include <string_view>

#include "assets/car_data.hpp"
#include "assets/driver_look.hpp"
#include "assets/driver_model.hpp"
#include "assets/driver_parts.hpp"
#include "assets/geometry_asset.hpp"
#include "assets/material_library.hpp"
#include "assets/part_library.hpp"
#include "assets/piece_catalog.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "sim/physics/chassis.hpp"

namespace rr::app {

struct BuiltCarModel {
    assets::GeometryAsset geometry;
    sim::CarBuild build;
};

/// Its materials are the pieces' own `MDB`, `LPIECELO` for races and `LPIECEHI` for menus.
[[nodiscard]] Result<BuiltCarModel> BuildCarModel(const assets::Vfs& vfs,
                                                  const assets::CarData& car,
                                                  assets::PieceDetail detail);

[[nodiscard]] Result<assets::DriverPartCatalog> LoadDriverParts(const assets::Vfs& vfs);

[[nodiscard]] Result<assets::PartLibrary> LoadHeadParts(const assets::Vfs& vfs,
                                                        std::string_view path);

/// `BODYPART`'s materials and textures, with the looks of the parts mods add in `PARTS.TOML`. A mod
/// part's look copies the first part's material, and one that doesn't load is logged and skipped.
[[nodiscard]] Result<assets::TrackAssets> LoadDriverLooks(const assets::Vfs& vfs,
                                                          assets::TrackLoader& loader,
                                                          const assets::DriverPartCatalog& parts);

/// Menus build on `MENUPART` bodies and races on `GAMEPART`'s `leg_box`.
[[nodiscard]] Result<assets::GeometryAsset> BuildMinifig(const assets::Vfs& vfs,
                                                         const assets::DriverPartCatalog& parts,
                                                         const assets::DriverLook& look,
                                                         assets::DriverDetail detail);

/// `body` names a `MENUPART` body. Its `RIG` sidecar, if any, comes along.
[[nodiscard]] Result<assets::SkeletonAsset> LoadMenuMinifigSkeleton(const assets::Vfs& vfs,
                                                                    std::string_view body);

[[nodiscard]] Result<assets::SkeletonAsset> LoadMenuMinifigSkeleton(
    const assets::Vfs& vfs, const assets::DriverPartCatalog& parts, const assets::DriverLook& look);

/// The `ADB` that poses the menu minifig's body when nothing else animates it.
[[nodiscard]] Result<std::string> MenuMinifigAnimations(const assets::DriverPartCatalog& parts,
                                                        const assets::DriverLook& look);

/// The face's five other expressions, copied from its default material with `PARTDB` images.
[[nodiscard]] Result<assets::TrackAssets> LoadExpressionFaces(
    const assets::Vfs& vfs, const assets::NameMap<assets::MaterialAsset>& materials,
    std::string_view face);

/// Sets ambient and diffuse to 200 grey on the look's default face, torso and legs materials.
void GreyMinifigMaterials(assets::NameMap<assets::MaterialAsset>& materials,
                          const assets::DriverPartCatalog& parts, const assets::DriverLook& worn);

}
