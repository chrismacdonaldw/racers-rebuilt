#pragma once

#include <optional>

#include "app/track_model.hpp"
#include "assets/driver_look.hpp"
#include "assets/skeleton_database.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"
#include "settings/racers.hpp"

namespace rr::app {

// NOLINTNEXTLINE(bugprone-exception-escape)
struct StandRacer {
    assets::SkeletonAsset skeleton;
    float scale = 1.0F;
    JointedModel driver;
    std::optional<JointedModel> car;
};

[[nodiscard]] Result<StandRacer> BuildStandDriver(render::Renderer& renderer,
                                                  const assets::Vfs& vfs,
                                                  assets::TrackLoader& loader,
                                                  const assets::DriverLook& look);

/// The racer's menu minifig on its body rig, and its menu car unless it has no pieces.
[[nodiscard]] Result<StandRacer> BuildStandRacer(render::Renderer& renderer, const assets::Vfs& vfs,
                                                 assets::TrackLoader& loader,
                                                 const settings::Racer& racer);

}
