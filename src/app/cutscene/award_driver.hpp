#pragma once

#include <string>

#include "app/cutscene/cutscene_play.hpp"
#include "assets/driver_database.hpp"
#include "assets/driver_look.hpp"
#include "assets/geometry_asset.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

// NOLINTNEXTLINE(bugprone-exception-escape)
struct AwardDriver {
    assets::DriverLook look;
    std::string face;
    assets::GeometryAsset minifig;
    assets::TrackAssets looks;
};

[[nodiscard]] assets::DriverLook DriverLookOf(const assets::DriverEntry& driver);

/// The player's own racer when `play` has one, else the prebuilt driver `play.driver` names.
[[nodiscard]] Result<assets::DriverLook> AwardLook(const assets::Vfs& vfs,
                                                   const CutscenePlay& play);

/// The `MENUPART` minifig with its materials and every expression of its face.
[[nodiscard]] Result<AwardDriver> LoadAwardDriver(const assets::Vfs& vfs,
                                                  assets::TrackLoader& loader,
                                                  const assets::DriverLook& look);

/// Swaps the driver in for `guy1` and its legs onto `swap` and `pleg`. Its materials win on a name.
void DressAwardDriver(const AwardDriver& driver, assets::TrackAssets& assets);

}
