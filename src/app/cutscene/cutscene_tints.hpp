#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "app/cutscene/cutscene_bindings.hpp"
#include "assets/cutscene_effects.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// `targets[i]` is the entity `tints[i]` colours, unset for a tint without one.
class CutsceneTints {
public:
    CutsceneTints(std::span<const assets::CutsceneTint> tints,
                  std::vector<std::optional<ModelBinding>> targets);

    void Start(std::size_t tint) { startOrder_.at(tint) = ++starts_; }
    void Stop(std::size_t tint) { startOrder_.at(tint) = 0; }
    void StopAll();

    /// Recolours `draws`, which belong to `model`, with the newest running tint on its entity.
    void Apply(const ModelBinding& model, std::span<render::MeshDraw> draws) const;

private:
    std::vector<render::VertexColorTransform> colors_;
    std::vector<std::optional<ModelBinding>> targets_;
    std::vector<std::uint64_t> startOrder_;
    std::uint64_t starts_ = 0;
};

[[nodiscard]] Result<std::vector<std::optional<ModelBinding>>> BindTints(
    std::span<const assets::CutsceneTint> tints, std::span<const assets::WorldAsset* const> worlds,
    std::span<const NamedDraws> statics, const sim::TrackEntitySources& entities);

}
