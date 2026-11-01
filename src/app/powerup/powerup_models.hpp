#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_draws.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "assets/world_database.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "render/renderer.hpp"

namespace rr::app {

struct EffectModelFiles {
    std::string geometry;
    std::optional<JointedAssets> jointed;
    Transform placement;
    assets::TextureScroll scroll;
};

/// Entities are keyed by uppercase name, or by model name for the one unnamed entity.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct PowerupAssets {
    assets::TrackAssets models;
    assets::NameMap<EffectModelFiles> entities;
};

/// The model and animated entities of a world in `GAMEDATA/COMMON`, such as `POWERUP.WDB`.
[[nodiscard]] Result<PowerupAssets> LoadEffectAssets(const assets::Vfs& vfs,
                                                     assets::TrackLoader& loader,
                                                     std::string_view world);

[[nodiscard]] Result<PowerupAssets> LoadPowerupAssets(const assets::Vfs& vfs,
                                                      assets::TrackLoader& loader);

struct EffectModel {
    ModelDraws draws;
    const JointedAssets* jointed = nullptr;
    Transform placement;
    assets::TextureScroll scroll;
};

using EffectModels = assets::NameMap<EffectModel>;

/// `assets` must outlive the models.
[[nodiscard]] Result<EffectModels> UploadPowerupModels(render::Renderer& renderer,
                                                       const PowerupAssets& assets);

// NOLINTNEXTLINE(bugprone-exception-escape)
struct PowerupModelSet {
    std::unique_ptr<PowerupAssets> assets;
    EffectModels models;
    assets::NameMap<render::Material> quadMaterials;
};

/// Logs a failure and returns nothing, so a race runs without power-up visuals.
[[nodiscard]] PowerupModelSet LoadPowerupModels(render::Renderer& renderer, const assets::Vfs& vfs,
                                                assets::TrackLoader& loader);

}
