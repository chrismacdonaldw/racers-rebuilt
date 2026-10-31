#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_visuals.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/bounding_sphere.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// The prebuilt racer in each grid slot of a track's circuit.
class RacerGrid {
public:
    /// Fails when the circuit has fewer than `slots` drivers. The driver of slot `playerRacer`
    /// swaps places with slot 0's. `renderer` must outlive the grid.
    [[nodiscard]] static Result<RacerGrid> Load(render::Renderer& renderer, const assets::Vfs& vfs,
                                                assets::TrackLoader& loader, std::string_view track,
                                                std::size_t slots, std::size_t playerRacer);

    [[nodiscard]] sim::ChassisSpec Chassis(std::size_t slot) const;

    [[nodiscard]] BoundingSphere LocalBounds(std::size_t slot) const {
        return slots_.at(slot).localBounds;
    }

    /// The slot driver's line in `DRIVERS.SRF`.
    [[nodiscard]] std::int32_t NameIndex(std::size_t slot) const;

    /// `poses` and `motion` hold one engine-space entry per slot, in slot order.
    void Update(std::span<const sim::EntitySnapshot> poses,
                std::span<const sim::RacerSnapshot> motion, float elapsedMs);

    /// Every slot's draws, in slot order. The count never changes.
    [[nodiscard]] std::span<const render::MeshDraw> Draws() const { return draws_; }

private:
    struct Slot {
        std::unique_ptr<RacerAssets> assets;
        RacerVisuals visuals;
        BoundingSphere localBounds;
    };

    explicit RacerGrid(std::vector<Slot> slots);

    std::vector<Slot> slots_;
    std::vector<render::MeshDraw> draws_;
};

}
