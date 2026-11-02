#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "app/track_model.hpp"
#include "assets/cutscene_timeline.hpp"
#include "assets/world_database.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

/// Indexed models are WDB sprites, which bind to nothing until sprites are drawn.
struct ModelBinding {
    std::optional<DrawSpan> staticDraws;
    std::optional<std::size_t> jointed;
};

/// Follows `node` of `entity` when there is one, or stands at `placement`, in game space.
struct CameraBinding {
    std::optional<std::size_t> entity;
    std::size_t node = 0;
    Transform placement;
    float verticalFovDegrees = 0.0F;
    float nearPlane = 0.0F;
    float farPlane = 0.0F;
};

struct CutsceneBindings {
    std::vector<ModelBinding> models;
    std::vector<CameraBinding> cameras;
};

/// Searches the worlds in order, ignoring case. Fails on a name no world has, as the original does.
[[nodiscard]] Result<CutsceneBindings> BindCutscene(
    const assets::CutsceneFrame& frame, std::span<const assets::WorldAsset* const> worlds,
    std::span<const NamedDraws> statics, const sim::TrackEntitySources& entities);

}
