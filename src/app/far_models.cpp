#include "app/far_models.hpp"

#include <algorithm>

namespace rr::app {

void FarModels::SetHidden(bool hidesCars, bool hidesScenery) {
    hidesCars_ = hidesCars;
    hidesScenery_ = hidesScenery;
}

void FarModels::Reset(std::span<render::MeshDraw> meshes) {
    Restore(meshes);
    hidden_.clear();
    cars_.clear();
}

void FarModels::AddCar(const FarModel& car) {
    if (hidesCars_) {
        cars_.push_back(car);
    }
}

std::span<const render::MeshDraw> FarModels::Cull(std::size_t view, const Vector3& camera,
                                                  std::vector<render::MeshDraw>& meshes) {
    if (!hidesCars_ && !hidesScenery_) {
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
    Hide(camera, cars_, meshes, logs);
    if (hidesScenery_) {
        Hide(camera, scenery_, meshes, logs);
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
