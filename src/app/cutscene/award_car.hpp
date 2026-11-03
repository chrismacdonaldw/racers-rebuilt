#pragma once

#include <optional>

#include "app/cutscene/cutscene_play.hpp"
#include "assets/geometry_asset.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "settings/racers.hpp"

namespace rr::app {

// NOLINTNEXTLINE(bugprone-exception-escape)
struct AwardCar {
    assets::GeometryAsset model;
    assets::TrackAssets looks;
};

/// The champion's `CHAMP` quick build on the car win screen, else the player's own racer.
[[nodiscard]] Result<std::optional<settings::Racer>> AwardCarRacer(const assets::Vfs& vfs,
                                                                   const CutscenePlay& play);

/// The menu car on its `PIECEDB` chassis frozen at rest, or none when the car has no pieces.
[[nodiscard]] Result<std::optional<AwardCar>> LoadAwardCar(const assets::Vfs& vfs,
                                                           assets::TrackLoader& loader,
                                                           const settings::Racer& racer);

[[nodiscard]] bool HasCarBody(const assets::TrackAssets& assets);

/// Swaps the car in for `carbody`'s model. Its materials win on a name.
void DressAwardCar(const AwardCar& car, assets::TrackAssets& assets);

}
