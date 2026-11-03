#pragma once

#include "app/racer/champion_car.hpp"
#include "app/racer/racer_assets.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "settings/racers.hpp"

namespace rr::app {

/// The car from `LPIECELO`, and the minifig from `GAMEPART` with the default face. A champion's
/// body takes the car's place but not its physics.
[[nodiscard]] Result<RacerAssets> LoadBuiltRacer(const assets::Vfs& vfs,
                                                 assets::TrackLoader& loader,
                                                 const assets::RacerCatalog& catalog,
                                                 const settings::Racer& racer,
                                                 CarBody carBody = CarBody::Bricks);

}
