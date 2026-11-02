#include "app/cutscene/cutscene_player.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "app/track_space.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

constexpr float kChannelMax = 255.0F;

render::Color ToColor(const assets::CutsceneColor& color) {
    return {.r = static_cast<float>(color[0]) / kChannelMax,
            .g = static_cast<float>(color[1]) / kChannelMax,
            .b = static_cast<float>(color[2]) / kChannelMax};
}

Transform Placement(const assets::CutsceneModel& model) {
    return {.position = model.position,
            .rotation = sim::ToQuaternion(sim::OrthonormalBasis(model.direction, model.up))};
}

}

CutscenePlayer::CutscenePlayer(const assets::CutsceneFrame& frame, CutsceneBindings bindings,
                               CutsceneLooks looks, sim::TrackEntitySources& sources,
                               CutsceneStatics statics, bool isLooping)
    : frame_(&frame),
      bindings_(std::move(bindings)),
      materials_(std::move(looks.materials)),
      sprites_(std::move(looks.sprites)),
      sources_(&sources),
      statics_(statics),
      timeline_(frame, isLooping),
      entities_(sources) {
    for (const assets::CutsceneModel& model : frame.models) {
        placements_.push_back(Placement(model));
    }
    for (std::size_t entity = 0; entity < sources.entities.size(); ++entity) {
        entities_.Freeze(entity);
    }
}

std::span<const CutsceneEventChange> CutscenePlayer::Advance(Milliseconds elapsed) {
    const std::span<const CutsceneEventChange> changes = timeline_.Advance(elapsed);
    for (const CutsceneEventChange& change : changes) {
        if (change.begins) {
            Begin(change.event.index, change.event.kind);
        } else if (change.event.kind == CutsceneEventKind::Model) {
            materials_.End(change.event.index);
            if (const std::optional<std::size_t> jointed =
                    bindings_.models[change.event.index].jointed) {
                entities_.Freeze(*jointed);
            }
        }
    }
    materials_.Advance(elapsed);
    entities_.Tick(elapsed);
    return changes;
}

void CutscenePlayer::Begin(std::size_t event, CutsceneEventKind kind) {
    if (kind == CutsceneEventKind::Camera) {
        const std::optional<std::size_t> tracked = bindings_.cameras[event].entity;
        if (const std::optional<std::int32_t> part = frame_->cameras[event].part; tracked && part) {
            PlayPart(*tracked, *part, false);
        }
        return;
    }
    if (kind != CutsceneEventKind::Model) {
        return;
    }
    materials_.Begin(event);
    const std::optional<std::size_t> jointed = bindings_.models[event].jointed;
    if (!jointed) {
        return;
    }
    sources_->entities[*jointed].placement = placements_[event];
    if (const std::optional<std::int32_t> part = frame_->models[event].part) {
        PlayPart(*jointed, *part, true);
    }
}

void CutscenePlayer::PlayPart(std::size_t entity, std::int32_t part, bool animates) {
    const bool wasFrozen = entities_.IsFrozen(entity);
    entities_.Play(entity, static_cast<std::size_t>(part), true);
    if (!animates && wasFrozen) {
        entities_.Freeze(entity);
    }
}

std::optional<render::Camera> CutscenePlayer::ViewCamera(
    std::span<const sim::EntityClock> clocks) const {
    const std::optional<std::size_t> drawing = timeline_.DrawingCamera();
    if (!drawing) {
        return std::nullopt;
    }
    const CameraBinding& camera = bindings_.cameras[*drawing];
    Transform pose = camera.placement;
    if (camera.entity) {
        const sim::EntityClock clock =
            *camera.entity < clocks.size() ? clocks[*camera.entity] : sim::EntityClock{};
        pose = sim::NodeWorld(*sources_, *camera.entity, clock, camera.node);
    }
    return render::Camera{.transform = ViewToEngine(pose),
                          .verticalFovDegrees = camera.verticalFovDegrees,
                          .nearPlane = camera.nearPlane,
                          .farPlane = camera.farPlane};
}

render::Lighting CutscenePlayer::Lighting(std::vector<render::DirectionalLight>& storage) const {
    storage.clear();
    for (const std::size_t light : timeline_.DirectionalLights()) {
        const assets::CutsceneDirectionalLight& event = frame_->directionalLights[light];
        storage.push_back({.direction = ToEngine(event.direction), .color = ToColor(event.color)});
    }
    const std::optional<std::size_t> ambient = timeline_.AmbientLight();
    return {.lights = storage,
            .ambient = ambient ? ToColor(frame_->ambientLights[*ambient].color)
                               : render::Color{.r = 0.0F, .g = 0.0F, .b = 0.0F}};
}

void CutscenePlayer::AppendDraws(const TrackEntityDraws& jointed,
                                 std::vector<render::MeshDraw>& out) const {
    for (std::size_t model = 0; model < bindings_.models.size(); ++model) {
        if (!timeline_.IsActive({.kind = CutsceneEventKind::Model, .index = model})) {
            continue;
        }
        const ModelBinding& binding = bindings_.models[model];
        if (binding.staticDraws) {
            const DrawSpan span = *binding.staticDraws;
            const std::size_t first = out.size();
            const Transform placed = TransformToEngine(placements_[model]);
            for (render::MeshDraw draw : statics_.draws.subspan(span.first, span.count)) {
                draw.transform = placed;
                draw.isStatic = false;
                out.push_back(draw);
            }
            materials_.Apply(model, std::span(out).subspan(first),
                             statics_.slots.subspan(span.first, span.count));
        }
        if (binding.jointed) {
            const DrawSpan span = jointed.SpanOf(*binding.jointed);
            const std::size_t first = out.size();
            const std::span<const render::MeshDraw> draws =
                jointed.Draws().subspan(span.first, span.count);
            out.insert(out.end(), draws.begin(), draws.end());
            materials_.Apply(model, std::span(out).subspan(first),
                             jointed.Slots().subspan(span.first, span.count));
        }
    }
}

void CutscenePlayer::AppendQuads(const Transform& camera,
                                 std::vector<render::QuadDraw>& out) const {
    for (std::size_t model = 0; model < bindings_.models.size(); ++model) {
        if (timeline_.IsActive({.kind = CutsceneEventKind::Model, .index = model})) {
            sprites_.Append(model, placements_[model], camera, out);
        }
    }
}

}
