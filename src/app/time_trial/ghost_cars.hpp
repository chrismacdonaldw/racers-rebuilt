#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "app/hud/minimap.hpp"
#include "app/powerup/powerup_models.hpp"
#include "app/racer/far_cars.hpp"
#include "app/racer/racer_grid.hpp"
#include "app/time_trial/ghost_car.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// The record ghost in `GHOST.WDB`'s car, and the best ghost in a copy of the player's.
// NOLINTNEXTLINE(bugprone-exception-escape)
class GhostCars {
public:
    /// Logs a failure and returns nothing, so the ghosts still race but aren't drawn.
    [[nodiscard]] static std::optional<GhostCars> Load(render::Renderer& renderer,
                                                       const assets::Vfs& vfs,
                                                       assets::TrackLoader& loader,
                                                       const RacerGrid& racers,
                                                       std::size_t playerSlot);

    void Update(const sim::GhostSnapshot& ghosts, float elapsedMs);

    /// Best ghost first, as the original draws them.
    void AppendDraws(std::vector<render::MeshDraw>& out) const;

    /// `firstDraw` is where `AppendDraws` starts in the scene's meshes.
    void AddFarCars(const sim::GhostSnapshot& ghosts, std::size_t firstDraw, FarCars& out) const;

private:
    GhostCars(std::unique_ptr<EffectAssets> recordAssets, GhostCar recordCar, GhostCar bestCar)
        : recordAssets_(std::move(recordAssets)),
          recordCar_(std::move(recordCar)),
          bestCar_(std::move(bestCar)) {}

    std::unique_ptr<EffectAssets> recordAssets_;
    GhostCar recordCar_;
    GhostCar bestCar_;
};

/// The best ghost takes a racer's marker and the record ghost the champion's.
void AppendGhostMarkers(const sim::GhostSnapshot& ghosts, std::vector<MapMarker>& out);

}
