#include "app/cutscene/cutscene_bindings.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "assets/game_path.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

bool SameName(std::string_view a, std::string_view b) {
    return assets::ToUpperAscii(a) == assets::ToUpperAscii(b);
}

bool HasModelEntity(std::span<const assets::WorldAsset* const> worlds, std::string_view name) {
    return std::ranges::any_of(worlds, [&](const assets::WorldAsset* world) {
        return std::ranges::any_of(world->modelEntities, [&](const assets::WorldEntity& entity) {
            return SameName(entity.name, name);
        });
    });
}

std::optional<std::size_t> FindJointed(const sim::TrackEntitySources& entities,
                                       std::string_view name) {
    const auto found = std::ranges::find_if(entities.entities, [&](const sim::TrackEntity& entity) {
        return SameName(entity.name, name);
    });
    if (found == entities.entities.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - entities.entities.begin());
}

Result<void> CheckPart(const sim::TrackEntitySources& entities, std::size_t entity,
                       std::optional<std::int32_t> part, std::string_view event) {
    const std::size_t parts = entities.animations[entities.entities[entity].animation].parts.size();
    if (part && (*part < 0 || std::cmp_greater_equal(*part, parts))) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} plays part {} of {}, which has {}", event, *part,
                                entities.entities[entity].name, parts));
    }
    return {};
}

Result<ModelBinding> BindName(const assets::CutsceneModel& event, const assets::EntityByName& model,
                              std::span<const assets::WorldAsset* const> worlds,
                              std::span<const NamedDraws> statics,
                              const sim::TrackEntitySources& entities) {
    if (model.isJointed) {
        const std::optional<std::size_t> jointed = FindJointed(entities, model.name);
        if (!jointed) {
            return Fail(ErrorCode::NotFound,
                        std::format("unable to find jointed model {}", model.name));
        }
        return CheckPart(entities, *jointed, event.part, event.name).transform([&] {
            return ModelBinding{.jointed = jointed};
        });
    }
    if (!HasModelEntity(worlds, model.name)) {
        return Fail(ErrorCode::NotFound, std::format("unable to find model {}", model.name));
    }
    const auto drawn = std::ranges::find_if(
        statics, [&](const NamedDraws& draws) { return SameName(draws.entity, model.name); });
    if (drawn == statics.end()) {
        return ModelBinding{};
    }
    return ModelBinding{.staticDraws = drawn->span};
}

Result<ModelBinding> BindIndex(const assets::EntityByIndex& model,
                               std::span<const assets::WorldAsset* const> worlds) {
    if (model.world >= worlds.size() || model.entity >= worlds[model.world]->sprites.size()) {
        return Fail(ErrorCode::NotFound,
                    std::format("sprite {} of world {} doesn't exist", model.entity, model.world));
    }
    return ModelBinding{.sprite = worlds[model.world]->sprites[model.entity]};
}

Transform StandingPose(const assets::WorldCamera& camera) {
    const Vector3 forward = camera.target ? *camera.target - camera.position : camera.forward;
    return {.position = camera.position,
            .rotation = sim::ToQuaternion(sim::OrthonormalBasis(forward, camera.up))};
}

Result<CameraBinding> BindCamera(const assets::CutsceneCamera& event,
                                 std::span<const assets::WorldAsset* const> worlds,
                                 const sim::TrackEntitySources& entities) {
    for (const assets::WorldAsset* world : worlds) {
        const auto camera = std::ranges::find_if(world->cameras, [&](const assets::WorldCamera& c) {
            return SameName(c.name, event.camera);
        });
        if (camera == world->cameras.end()) {
            continue;
        }
        CameraBinding bound{.node = static_cast<std::size_t>(std::max(camera->trackedNode, 0)),
                            .placement = StandingPose(*camera),
                            .verticalFovDegrees = camera->fovDegrees,
                            .nearPlane = camera->nearClip,
                            .farPlane = camera->farClip};
        if (camera->trackedEntity != assets::kNoIndex) {
            const std::string& tracked =
                world->animatedEntities.at(static_cast<std::size_t>(camera->trackedEntity)).name;
            bound.entity = FindJointed(entities, tracked);
            if (!bound.entity) {
                return Fail(ErrorCode::NotFound,
                            std::format("camera {} follows {}, which has no skeleton", event.camera,
                                        tracked));
            }
            return CheckPart(entities, *bound.entity, event.part, event.name).transform([&] {
                return bound;
            });
        }
        return bound;
    }
    return Fail(ErrorCode::NotFound, std::format("unable to find named camera {}", event.camera));
}

}

Result<CutsceneBindings> BindCutscene(const assets::CutsceneFrame& frame,
                                      std::span<const assets::WorldAsset* const> worlds,
                                      std::span<const NamedDraws> statics,
                                      const sim::TrackEntitySources& entities) {
    CutsceneBindings bindings;
    for (const assets::CutsceneModel& model : frame.models) {
        Result<ModelBinding> bound =
            std::holds_alternative<assets::EntityByName>(model.model)
                ? BindName(model, std::get<assets::EntityByName>(model.model), worlds, statics,
                           entities)
                : BindIndex(std::get<assets::EntityByIndex>(model.model), worlds);
        if (!bound) {
            return std::unexpected(std::move(bound.error()));
        }
        bindings.models.push_back(*bound);
    }
    for (const assets::CutsceneCamera& camera : frame.cameras) {
        Result<CameraBinding> bound = BindCamera(camera, worlds, entities);
        if (!bound) {
            return std::unexpected(std::move(bound.error()));
        }
        bindings.cameras.push_back(*bound);
    }
    return bindings;
}

}
