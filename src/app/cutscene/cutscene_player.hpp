#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "app/cutscene/cutscene_bindings.hpp"
#include "app/cutscene/cutscene_materials.hpp"
#include "app/cutscene/cutscene_sprites.hpp"
#include "app/cutscene/cutscene_timeline.hpp"
#include "app/cutscene/cutscene_tints.hpp"
#include "app/track_entity_draws.hpp"
#include "assets/cutscene_timeline.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

/// `slots[i]` is the material slot of `draws[i]`.
struct CutsceneStatics {
    std::span<const render::MeshDraw> draws;
    std::span<const std::uint32_t> slots;
};

struct CutsceneLooks {
    CutsceneMaterials materials;
    CutsceneSprites sprites;
};

/// Plays one `CDB` frame. The frame, `sources` and `statics` must outlive it.
class CutscenePlayer {
public:
    CutscenePlayer(const assets::CutsceneFrame& frame, CutsceneBindings bindings,
                   CutsceneLooks looks, sim::TrackEntitySources& sources, CutsceneStatics statics,
                   bool isLooping);

    /// Returns the events that began or ended, as `CutsceneTimeline::Advance` does.
    std::span<const CutsceneEventChange> Advance(Milliseconds elapsed);

    [[nodiscard]] bool IsComplete() const { return timeline_.IsComplete(); }
    [[nodiscard]] std::span<const sim::EntityClock> Clocks() const { return entities_.Clocks(); }

    /// Unset while no camera event is active.
    [[nodiscard]] std::optional<render::Camera> ViewCamera(
        std::span<const sim::EntityClock> clocks) const;

    /// The lights point into `storage`, which must outlive the returned value.
    [[nodiscard]] render::Lighting Lighting(std::vector<render::DirectionalLight>& storage) const;

    /// `jointed` must be posed. Leaves out models past their range from `rangeFrom`, if set.
    void AppendDraws(const TrackEntityDraws& jointed, const CutsceneTints& tints,
                     const std::optional<Vector3>& rangeFrom,
                     std::vector<render::MeshDraw>& out) const;

    /// Appends the active sprites, facing `camera`, which is in engine space.
    void AppendQuads(const Transform& camera, std::vector<render::QuadDraw>& out) const;

private:
    void Begin(std::size_t event, CutsceneEventKind kind);
    void PlayPart(std::size_t entity, std::int32_t part, bool animates);

    const assets::CutsceneFrame* frame_;
    CutsceneBindings bindings_;
    CutsceneMaterials materials_;
    CutsceneSprites sprites_;
    sim::TrackEntitySources* sources_;
    CutsceneStatics statics_;
    std::vector<Transform> placements_;
    CutsceneTimeline timeline_;
    sim::TrackEntities entities_;
};

}
