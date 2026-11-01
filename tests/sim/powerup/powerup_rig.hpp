#pragma once

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "../physics/plane_world.hpp"
#include "assets/brick_layout.hpp"
#include "assets/checkpoints.hpp"
#include "assets/target_points.hpp"
#include "core/bounding_sphere.hpp"
#include "sim/powerup/brick_field.hpp"
#include "sim/powerup/explosions.hpp"
#include "sim/powerup/green_effects.hpp"
#include "sim/powerup/powerup_system.hpp"
#include "sim/powerup/red_effects.hpp"
#include "sim/powerup/shield_effect.hpp"
#include "sim/powerup/weapon_events.hpp"
#include "sim/powerup/yellow_effects.hpp"

namespace rr::test {

inline constexpr Milliseconds kTick{12};
inline constexpr sim::ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
inline constexpr sim::RacerControls kThrottle{.isThrottleHeld = true};
inline constexpr float kGateSpacing = 400.0F;
inline constexpr int kWarpStartTicks = 42;
inline constexpr int kWarpTicks = kWarpStartTicks + 125;

inline constexpr float kReachRadius = 4.0F;
inline constexpr Vector3 kSpareWhite{.y = -1000.0F};

inline sim::EffectFactories AllEffects() {
    sim::EffectFactories factories;
    sim::AddGreenEffects(factories);
    sim::AddBlueEffects(factories);
    sim::AddRedEffects(factories);
    sim::AddYellowEffects(factories);
    return factories;
}

inline sim::CheckpointGraph Line() {
    std::vector<assets::Checkpoint> gates;
    for (std::uint8_t i = 0; i < 5; ++i) {
        const float x = static_cast<float>(i) * kGateSpacing;
        gates.push_back({.normal = {.x = -1.0F},
                         .distance = x,
                         .position = {.x = x},
                         .links = {static_cast<std::uint8_t>((i + 1) % 5), assets::kNoLink,
                                   assets::kNoLink, assets::kNoLink}});
    }
    Result<sim::CheckpointGraph> graph =
        sim::CheckpointGraph::Create({.checkpoints = std::move(gates)});
    REQUIRE(graph.has_value());
    return std::move(*graph);
}

/// A line of racers on flat ground, with gates every 400 units along +X and one spare white brick.
class Rig {
public:
    explicit Rig(std::size_t racerCount = 1)
        : progress_(racerCount, 3),
          bricks_(assets::BrickLayout{.white = {kSpareWhite}}),
          inventories_(racerCount),
          system_(AllEffects(), racerCount),
          reaches_(racerCount) {
        for (std::size_t i = 0; i < racerCount; ++i) {
            racers_.emplace_back(
                kChassis, sim::CarBuild{},
                sim::RacerStart{
                    .position = {.x = 5.0F, .y = static_cast<float>(i) * 20.0F, .z = 0.2F}});
        }
        positions_.resize(racerCount);
        MeasureReaches();
    }

    void Place(std::size_t racer, const Vector3& position) {
        racers_[racer].Place({.position = position});
        positions_[racer] = racers_[racer].Body().motion.position;
        MeasureReaches();
    }

    void AimAt(std::vector<assets::TargetPoint> targets) { targets_ = std::move(targets); }

    void GiveWhite(std::size_t racer) { REQUIRE(inventories_[racer].TakeWhite(0)); }

    void CrossFirstGate() {
        for (std::size_t i = 0; i < racers_.size(); ++i) {
            const Vector3 at = racers_[i].Body().motion.position;
            progress_.Move(graph_, i,
                           {.from = at - Vector3{.x = 10.0F}, .to = at, .facing = {.x = 1.0F}},
                           kTick, true);
        }
    }

    void Fire(assets::BrickColour colour, std::uint8_t level, std::uint8_t racer = 0) {
        system_.BeginTick();
        weapons_.Clear();
        sim::PowerupContext context = Context();
        system_.Fire({.racer = racer, .colour = colour, .level = level}, context);
    }

    void TickFor(int ticks, const sim::RacerControls& controls = {}) {
        for (int tick = 0; tick < ticks; ++tick) {
            Tick(controls);
        }
    }

    void Tick(const sim::RacerControls& controls = kThrottle) {
        for (std::size_t i = 0; i < racers_.size(); ++i) {
            racers_[i].Tick(kTick, controls, world_);
            const sim::BodyState& body = racers_[i].Body();
            progress_.Move(
                graph_, i,
                {.from = positions_[i], .to = body.motion.position, .facing = body.facing}, kTick,
                true);
            positions_[i] = body.motion.position;
        }
        system_.BeginTick();
        weapons_.Clear();
        MeasureReaches();
        sim::PowerupContext context = Context();
        system_.Tick(kTick, context);
        explosions_.Tick(kTick, racers_, reaches_);
        spikes_.Tick(kTick, racers_, reaches_);
        for (sim::Racer& racer : racers_) {
            racer.UpdateTimers(kTick);
        }
    }

    int TicksUntilIdle() {
        int ticks = 0;
        while (!system_.Active().empty()) {
            Tick();
            ++ticks;
        }
        return ticks;
    }

    template <typename Done>
    int TicksUntil(Done done, const sim::RacerControls& controls = {}, int limit = 1000) {
        for (int ticks = 1; ticks <= limit; ++ticks) {
            Tick(controls);
            if (done()) {
                return ticks;
            }
        }
        return -1;
    }

    sim::Racer& Car(std::size_t racer = 0) { return racers_[racer]; }

    [[nodiscard]] bool HasWeaponEvent(sim::WeaponEventKind kind) const {
        return std::ranges::any_of(weapons_.Events(), [kind](const sim::WeaponEvent& event) {
            return event.kind == kind;
        });
    }
    [[nodiscard]] const sim::PowerupSystem& System() const { return system_; }
    [[nodiscard]] const sim::RaceProgress& Progress() const { return progress_; }
    [[nodiscard]] const sim::BrickField& Bricks() const { return bricks_; }
    [[nodiscard]] const sim::PowerupInventory& Inventory(std::size_t racer) const {
        return inventories_[racer];
    }
    [[nodiscard]] const sim::Explosions& Blasts() const { return explosions_; }
    [[nodiscard]] const sim::Explosions& Spikes() const { return spikes_; }
    [[nodiscard]] std::span<const sim::WeaponEvent> Weapons() const { return weapons_.Events(); }

    [[nodiscard]] Vector3 EffectAt(std::size_t active = 0) const {
        return system_.Slots()[system_.Active()[active]].effect->Pose().position;
    }

private:
    sim::PowerupContext Context() {
        return {.racers = racers_,
                .world = &world_,
                .random = &random_,
                .checkpoints = &graph_,
                .progress = &progress_,
                .reaches = reaches_,
                .powerups = &system_,
                .bricks = &bricks_,
                .inventories = inventories_,
                .explosions = &explosions_,
                .spikes = &spikes_,
                .weapons = &weapons_,
                .targets = targets_};
    }

    void MeasureReaches() {
        for (std::size_t i = 0; i < racers_.size(); ++i) {
            reaches_[i] = {.centre = racers_[i].Body().motion.position, .radius = kReachRadius};
        }
    }

    sim::CheckpointGraph graph_ = Line();
    PlaneWorld world_ = FlatGround();
    std::vector<sim::Racer> racers_;
    std::vector<Vector3> positions_;
    sim::RaceProgress progress_;
    sim::BrickField bricks_;
    std::vector<sim::PowerupInventory> inventories_;
    sim::PowerupSystem system_;
    std::vector<BoundingSphere> reaches_;
    sim::Explosions explosions_;
    sim::Explosions spikes_{sim::kSpikeExplosion, sim::kSpikeExplosionCount};
    sim::WeaponEvents weapons_{64};
    sim::Random random_{1};
    std::vector<assets::TargetPoint> targets_;
};

inline sim::RouteEffects RouteEffectsOf(const sim::Racer& racer) {
    const std::optional<sim::AiRacer>& route = racer.Route();
    return route ? route->Effects() : sim::RouteEffects{};
}

inline std::size_t CountEnded(const sim::PowerupSystem& system) {
    std::size_t ended = 0;
    for (const sim::PowerupEvent& event : system.Events()) {
        ended += event.kind == sim::PowerupEventKind::Ended ? 1 : 0;
    }
    return ended;
}

}
