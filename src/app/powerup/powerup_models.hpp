#pragma once

#include <memory>
#include <optional>
#include <string>

#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_draws.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

struct EffectModelFiles {
    std::string geometry;
    std::optional<JointedAssets> jointed;
};

/// Entities are keyed by uppercase name, or by model name for the one unnamed entity.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct PowerupAssets {
    assets::TrackAssets models;
    assets::NameMap<EffectModelFiles> entities;
};

/// The model and animated entities of `GAMEDATA/COMMON/POWERUP.WDB`.
[[nodiscard]] Result<PowerupAssets> LoadPowerupAssets(const assets::Vfs& vfs,
                                                      assets::TrackLoader& loader);

struct EffectModel {
    ModelDraws draws;
    const JointedAssets* jointed = nullptr;
};

using EffectModels = assets::NameMap<EffectModel>;

/// `assets` must outlive the models.
[[nodiscard]] Result<EffectModels> UploadPowerupModels(render::Renderer& renderer,
                                                       const PowerupAssets& assets);

// NOLINTNEXTLINE(bugprone-exception-escape)
struct PowerupModelSet {
    std::unique_ptr<PowerupAssets> assets;
    EffectModels models;
};

/// Logs a failure and returns no models, so a race runs without them.
[[nodiscard]] PowerupModelSet LoadPowerupModels(render::Renderer& renderer, const assets::Vfs& vfs,
                                                assets::TrackLoader& loader);

}
