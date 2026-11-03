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

[[nodiscard]] std::vector<std::size_t> DrawsPerSlot(const RacerGrid& racers);

class FarCars {
public:
    explicit FarCars(std::vector<std::size_t> drawsPerSlot);

    void SetHidden(bool isHidden) { isHidden_ = isHidden; }
    [[nodiscard]] bool IsHidden() const { return isHidden_; }

    /// `meshes` holds every slot's car draws from `firstCar`. The result lasts until the next call.
    [[nodiscard]] std::span<const render::MeshDraw> Cull(std::size_t view, const Vector3& camera,
                                                         std::span<const sim::EntitySnapshot> poses,
                                                         std::span<const render::MeshDraw> meshes,
                                                         std::size_t firstCar);

private:
    std::vector<std::size_t> drawsPerSlot_;
    bool isHidden_ = false;
    std::array<std::vector<render::MeshDraw>, sim::kMaxPlayers> views_;
};

}
