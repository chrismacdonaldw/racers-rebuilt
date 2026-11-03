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

enum class FarKind : std::uint8_t { Car, Scenery };

class FarModels {
public:
    explicit FarModels(std::vector<FarModel> scenery = {});

    void SetHidden(bool hidesCars, bool hidesScenery);
    [[nodiscard]] bool HidesCars() const { return GroupOf(FarKind::Car).isHidden; }

    void Keep(FarKind kind, const FarModel& model);

    /// Gives back the index counts the first view hid, and forgets the models added since.
    void Reset(std::span<render::MeshDraw> meshes);
    void Add(FarKind kind, const FarModel& model);
    /// `models` count their draws from `firstDraw` in the scene's meshes.
    void Add(FarKind kind, std::span<const FarModel> models, std::size_t firstDraw);

    /// The first view culls `meshes` itself and later views a copy, kept until their next call.
    [[nodiscard]] std::span<const render::MeshDraw> Cull(std::size_t view, const Vector3& camera,
                                                         std::vector<render::MeshDraw>& meshes);

private:
    struct Group {
        std::vector<FarModel> kept;
        std::vector<FarModel> added;
        bool isHidden = false;
    };

    struct HiddenDraw {
        std::size_t draw = 0;
        std::uint32_t indexCount = 0;
    };

    [[nodiscard]] Group& GroupOf(FarKind kind) {
        return groups_.at(static_cast<std::size_t>(kind));
    }
    [[nodiscard]] const Group& GroupOf(FarKind kind) const {
        return groups_.at(static_cast<std::size_t>(kind));
    }
    void Restore(std::span<render::MeshDraw> meshes) const;
    void Hide(const Vector3& camera, std::span<render::MeshDraw> meshes, bool logs);
    void Hide(const Vector3& camera, std::span<const FarModel> models,
              std::span<render::MeshDraw> meshes, bool logs);

    std::array<Group, 2> groups_;
    std::vector<HiddenDraw> hidden_;
    std::array<std::vector<render::MeshDraw>, sim::kMaxPlayers> copies_;
};

}
