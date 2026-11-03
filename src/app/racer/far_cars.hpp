#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "app/racer/racer_grid.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"
#include "sim/input_frame.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// Squared, from the camera. The original draws a car and casts its shadow only within it.
inline constexpr float kCarRangeSquared = 250000.0F;

/// `position` is in engine space. `draws` are the car's own, which give back its index counts.
struct FarCar {
    Vector3 position;
    std::size_t firstDraw = 0;
    std::span<const render::MeshDraw> draws;
    float rangeSquared = kCarRangeSquared;
};

class FarCars {
public:
    void SetHidden(bool isHidden) { isHidden_ = isHidden; }
    [[nodiscard]] bool IsHidden() const { return isHidden_; }

    void Clear() { cars_.clear(); }
    void Add(const FarCar& car) { cars_.push_back(car); }

    /// The first view culls `meshes` itself and later views a copy, kept until their next call.
    [[nodiscard]] std::span<const render::MeshDraw> Cull(std::size_t view, const Vector3& camera,
                                                         std::vector<render::MeshDraw>& meshes);

private:
    void Patch(const Vector3& camera, std::span<render::MeshDraw> meshes) const;

    std::vector<FarCar> cars_;
    bool isHidden_ = false;
    std::array<std::vector<render::MeshDraw>, sim::kMaxPlayers> copies_;
};

/// The grid's draws start at `firstDraw` in the scene's meshes, in slot order.
void AddGridCars(const RacerGrid& racers, std::span<const sim::EntitySnapshot> poses,
                 std::size_t firstDraw, FarCars& out);

}
