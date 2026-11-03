#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "app/track_model.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

class FarModels {
public:
    explicit FarModels(std::vector<FarModel> scenery = {}) : scenery_(std::move(scenery)) {}

    void SetHidden(bool hidesCars, bool hidesScenery);
    [[nodiscard]] bool HidesCars() const { return hidesCars_; }

    /// Gives back the index counts the first view hid, and forgets the cars.
    void Reset(std::span<render::MeshDraw> meshes);
    void AddScenery(const FarModel& model) { scenery_.push_back(model); }
    void AddCar(const FarModel& car);

    /// The first view culls `meshes` itself and later views a copy, kept until their next call.
    [[nodiscard]] std::span<const render::MeshDraw> Cull(std::size_t view, const Vector3& camera,
                                                         std::vector<render::MeshDraw>& meshes);

private:
    struct HiddenDraw {
        std::size_t draw = 0;
        std::uint32_t indexCount = 0;
    };

    void Restore(std::span<render::MeshDraw> meshes) const;
    void Hide(const Vector3& camera, std::span<render::MeshDraw> meshes, bool logs);
    void Hide(const Vector3& camera, std::span<const FarModel> models,
              std::span<render::MeshDraw> meshes, bool logs);

    std::vector<FarModel> scenery_;
    std::vector<FarModel> cars_;
    std::vector<HiddenDraw> hidden_;
    bool hidesCars_ = false;
    bool hidesScenery_ = false;
    std::array<std::vector<render::MeshDraw>, sim::kMaxPlayers> copies_;
};

}
