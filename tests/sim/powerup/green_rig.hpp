#pragma once

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "../physics/plane_world.hpp"
#include "assets/checkpoints.hpp"
#include "sim/powerup/green_effects.hpp"
#include "sim/powerup/powerup_system.hpp"

namespace rr::test {

inline constexpr Milliseconds kTick{12};
inline constexpr sim::ChassisSpec kChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};
inline constexpr sim::RacerControls kThrottle{.isThrottleHeld = true};
inline constexpr float kGateSpacing = 400.0F;
inline constexpr int kWarpStartTicks = 42;
inline constexpr int kWarpTicks = kWarpStartTicks + 125;

inline sim::EffectFactories GreenEffects() {
    sim::EffectFactories factories;
    sim::AddGreenEffects(factories);
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

/// A line of racers on flat ground, with gates every 400 units along +X.
class Rig {
public:
    explicit Rig(std::size_t racerCount = 1)
        : progress_(racerCount, 3), system_(GreenEffects(), racerCount) {
        for (std::size_t i = 0; i < racerCount; ++i) {
            racers_.emplace_back(
                kChassis, sim::CarBuild{},
                sim::RacerStart{
                    .position = {.x = 5.0F, .y = static_cast<float>(i) * 20.0F, .z = 0.2F}});
        }
        positions_.resize(racerCount);
    }

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
        sim::PowerupContext context = Context();
        system_.Tick(kTick, context);
    }

    int TicksUntilIdle() {
        int ticks = 0;
        while (!system_.Active().empty()) {
            Tick();
            ++ticks;
        }
        return ticks;
    }

    sim::Racer& Car(std::size_t racer = 0) { return racers_[racer]; }
    [[nodiscard]] const sim::PowerupSystem& System() const { return system_; }
    [[nodiscard]] const sim::RaceProgress& Progress() const { return progress_; }

private:
    sim::PowerupContext Context() {
        return {.racers = racers_,
                .world = &world_,
                .random = &random_,
                .checkpoints = &graph_,
                .progress = &progress_};
    }

    sim::CheckpointGraph graph_ = Line();
    PlaneWorld world_ = FlatGround();
    std::vector<sim::Racer> racers_;
    std::vector<Vector3> positions_;
    sim::RaceProgress progress_;
    sim::PowerupSystem system_;
    sim::Random random_{1};
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
