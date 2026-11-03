#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "app/race_scene_name.hpp"
#include "app/racer/car_look.hpp"
#include "app/racer/driver_name.hpp"
#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_draws.hpp"
#include "app/racer/racer_visuals.hpp"
#include "assets/chassis_database.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/bounding_sphere.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/ai/powerup_habits.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// The racer in each grid slot of a track's circuit, prebuilt but for a player's own.
class RacerGrid {
public:
    /// Takes up to `slots` drivers and swaps the player's slot with 0. `renderer` must outlive it.
    [[nodiscard]] static Result<RacerGrid> Load(render::Renderer& renderer, const assets::Vfs& vfs,
                                                assets::TrackLoader& loader,
                                                const RaceSceneSetup& setup, std::size_t slots);

    [[nodiscard]] std::size_t Count() const { return slots_.size(); }
    [[nodiscard]] sim::ChassisSpec Chassis(std::size_t slot) const;
    [[nodiscard]] sim::CarBuild Build(std::size_t slot) const;
    [[nodiscard]] const assets::SkidMarks& Skids(std::size_t slot) const;
    [[nodiscard]] sim::PowerupHabits Habits(std::size_t slot) const;

    [[nodiscard]] BoundingSphere LocalBounds(std::size_t slot) const {
        return slots_.at(slot).localBounds;
    }

    [[nodiscard]] DriverName Name(std::size_t slot) const;

    /// In the game's numbering, as from `DriverVoice`.
    [[nodiscard]] std::int32_t Voice(std::size_t slot) const;

    [[nodiscard]] float EnginePitch(std::size_t slot) const;

    /// Each span holds one entry per slot, poses in engine space. Ghosts are hidden.
    void Update(std::span<const sim::EntitySnapshot> poses,
                std::span<const sim::RacerSnapshot> motion, std::span<const CarLook> looks,
                float elapsedMs);

    /// Every slot's draws, in slot order. The count never changes.
    [[nodiscard]] std::span<const render::MeshDraw> Draws() const { return draws_; }

    [[nodiscard]] const RacerAssets& Assets(std::size_t slot) const {
        return *slots_.at(slot).assets;
    }

    /// The slot's car models at their origin, to draw a copy of it.
    [[nodiscard]] const RacerDraws& Models(std::size_t slot) const {
        return slots_.at(slot).models;
    }

    /// Shown even while the slot is a ghost.
    [[nodiscard]] std::span<const render::MeshDraw> SlotDraws(std::size_t slot) const {
        return slots_.at(slot).visuals.Draws();
    }

private:
    struct Slot {
        std::unique_ptr<RacerAssets> assets;
        RacerDraws models;
        RacerVisuals visuals;
        BoundingSphere localBounds;
    };

    explicit RacerGrid(std::vector<Slot> slots);

    std::vector<Slot> slots_;
    std::vector<render::MeshDraw> draws_;
};

}
