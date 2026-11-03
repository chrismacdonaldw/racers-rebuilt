#include "app/far_models.hpp"

#include <algorithm>

namespace rr::app {

FarModels::FarModels(std::vector<FarModel> scenery) {
    GroupOf(FarKind::Scenery).kept = std::move(scenery);
}

void FarModels::SetHidden(bool hidesCars, bool hidesScenery) {
    GroupOf(FarKind::Car).isHidden = hidesCars;
    GroupOf(FarKind::Scenery).isHidden = hidesScenery;
}

void FarModels::Keep(FarKind kind, const FarModel& model) {
    GroupOf(kind).kept.push_back(model);
}

void FarModels::Reset(std::span<render::MeshDraw> meshes) {
    Restore(meshes);
    hidden_.clear();
    for (Group& group : groups_) {
        group.added.clear();
    }
}

void FarModels::Add(FarKind kind, const FarModel& model) {
    if (Group& group = GroupOf(kind); group.isHidden) {
        group.added.push_back(model);
    }
}

void FarModels::Add(FarKind kind, std::span<const FarModel> models, std::size_t firstDraw) {
    for (FarModel model : models) {
        model.draws.first += firstDraw;
        Add(kind, model);
    }
}

std::span<const render::MeshDraw> FarModels::Cull(std::size_t view, const Vector3& camera,
                                                  std::vector<render::MeshDraw>& meshes) {
    if (std::ranges::none_of(groups_, &Group::isHidden)) {
        return meshes;
    }
    if (view == 0) {
        Hide(camera, meshes, true);
        return meshes;
    }
    std::vector<render::MeshDraw>& copy = copies_.at(view);
    copy.assign(meshes.begin(), meshes.end());
    Restore(copy);
    Hide(camera, copy, false);
    return copy;
}

void FarModels::Restore(std::span<render::MeshDraw> meshes) const {
    for (const HiddenDraw& hidden : hidden_) {
        if (hidden.draw < meshes.size()) {
            meshes[hidden.draw].indexCount = hidden.indexCount;
        }
    }
}

void FarModels::Hide(const Vector3& camera, std::span<render::MeshDraw> meshes, bool logs) {
    for (const Group& group : groups_) {
        if (group.isHidden) {
            Hide(camera, group.kept, meshes, logs);
            Hide(camera, group.added, meshes, logs);
        }
    }
}

void FarModels::Hide(const Vector3& camera, std::span<const FarModel> models,
                     std::span<render::MeshDraw> meshes, bool logs) {
    for (const FarModel& model : models) {
        const Vector3 offset = model.position - camera;
        if (Dot(offset, offset) <= model.rangeSquared) {
            continue;
        }
        const std::size_t end = std::min(model.draws.first + model.draws.count, meshes.size());
        for (std::size_t draw = model.draws.first; draw < end; ++draw) {
            if (logs && meshes[draw].indexCount != 0) {
                hidden_.push_back({.draw = draw, .indexCount = meshes[draw].indexCount});
            }
            meshes[draw].indexCount = 0;
        }
    }
}

}
