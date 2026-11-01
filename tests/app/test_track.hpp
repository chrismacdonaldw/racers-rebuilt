#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "app/frame_loop.hpp"
#include "assets/brick_layout.hpp"
#include "core/duration.hpp"
#include "race_drive.hpp"
#include "sim/ai/route_choice.hpp"
#include "sim/ai/straight_route.hpp"
#include "sim/input_frame.hpp"
#include "sim/physics/plane_world.hpp"
#include "sim/powerup/every_effect.hpp"
#include "sim/powerup/powerup_kind.hpp"
#include "sim/race/checkpoint_ring.hpp"
#include "sim/race/race_session.hpp"
#include "sim/replay/replay.hpp"

namespace rr::test {

inline constexpr std::size_t kTestGridSlots = 6;
inline constexpr float kGridX = -20.0F;
inline constexpr float kLaneWidth = 4.0F;
inline constexpr float kGroundEndX = 90.0F;
inline constexpr float kWallY = 14.0F;
inline constexpr Milliseconds kUseEvery{1200};
inline constexpr Milliseconds kFirstGift{6000};
inline constexpr std::array kGiftColours{assets::BrickColour::Green, assets::BrickColour::Blue,
                                         assets::BrickColour::Red, assets::BrickColour::Yellow};

/// The ring of gates on flat ground that ends past the last gate, with a wall along one side.
/// Racers that drive off the end respawn. Grid slots sit close enough for their boxes to touch,
/// and bricks of every colour lie down both lanes.
struct TestTrack {
    sim::CheckpointGraph graph = Ring();
    PlaneWorld world{{
        {.maxX = kGroundEndX},
        {.normal = {.y = -1.0F}, .offset = -kWallY},
    }};
    std::array<sim::RacerStart, kTestGridSlots> grid = Grid();
    sim::GridRoutes routes = Routes();
    assets::BrickLayout bricks = Bricks();

    [[nodiscard]] sim::RaceCourse Course() const {
        return {.checkpoints = &graph,
                .grid = grid,
                .world = &world,
                .lowestTrackZ = -50.0F,
                .routes = &routes,
                .bricks = &bricks};
    }

    [[nodiscard]] static sim::RaceSetup Setup(std::uint64_t seed) {
        sim::RaceSetup setup{.track = "test", .seed = seed, .effects = sim::EveryEffect()};
        setup.racers.resize(kTestGridSlots);
        setup.racers.front().isPlayer = true;
        return setup;
    }

private:
    static Vector3 Slot(std::size_t slot) {
        const std::size_t row = slot / 2;
        return {.x = kGridX - (static_cast<float>(row) * 8.0F),
                .y = static_cast<float>(slot % 2) * -kLaneWidth,
                .z = 0.2F};
    }

    static std::array<sim::RacerStart, kTestGridSlots> Grid() {
        std::array<sim::RacerStart, kTestGridSlots> grid{};
        for (std::size_t i = 0; i < grid.size(); ++i) {
            grid[i].position = Slot(i);
        }
        return grid;
    }

    static sim::GridRoutes Routes() {
        sim::GridRoutes routes(kTestGridSlots);
        for (std::size_t i = 0; i < routes.size(); ++i) {
            routes[i].push_back(StraightRoute(Slot(i)));
        }
        return routes;
    }

    static assets::BrickLayout Bricks() {
        assets::BrickLayout layout;
        for (std::size_t i = 0; i < 8; ++i) {
            const float x = static_cast<float>(i) * 10.0F;
            const float y = static_cast<float>(i % 2) * -kLaneWidth;
            layout.coloured.push_back({.position = {.x = x, .y = y, .z = 1.0F},
                                       .colour = kGiftColours.at(i % kGiftColours.size())});
            layout.white.push_back({.x = x + 5.0F, .y = -kLaneWidth - y, .z = 1.0F});
        }
        return layout;
    }
};

/// Throttle, steering into the wall and the pack, a drift, braking, then flat out, using power-ups.
[[nodiscard]] inline sim::InputFrame ScriptedInput(std::uint64_t tick) {
    using enum sim::Action;
    const std::uint64_t ms = tick * static_cast<std::uint64_t>(app::kSimTick.count());
    sim::InputFrame input;
    const auto hold = [&](sim::Action action) { input.held.set(sim::InputFrame::Index(action)); };
    hold(Accelerate);
    if (ms % 9000 < 1500) {
        hold(SteerLeft);
    } else if (ms % 9000 < 2500) {
        hold(SteerRight);
        hold(Drift);
    } else if (ms % 9000 < 3200) {
        input.held.reset();
        hold(Brake);
        hold(SteerRight);
    }
    if (ms % static_cast<std::uint64_t>(kUseEvery.count()) < 100) {
        hold(UsePowerup);
    }
    return input;
}

/// Each colour at each level in turn, given to the player as every other use press comes round.
[[nodiscard]] inline std::vector<PowerupGift> EveryPowerup() {
    std::vector<PowerupGift> gifts;
    for (const assets::BrickColour colour : kGiftColours) {
        for (std::uint8_t level = 0; level <= sim::kMaxPowerupLevel; ++level) {
            const Milliseconds at = kFirstGift + (kUseEvery * 2 * std::ssize(gifts));
            gifts.push_back({.tick = static_cast<std::uint64_t>(at / app::kSimTick),
                             .colour = colour,
                             .level = level});
        }
    }
    return gifts;
}

[[nodiscard]] inline sim::Replay ScriptedReplay(Milliseconds duration) {
    const auto ticks = static_cast<std::uint64_t>(duration / app::kSimTick);
    std::vector<sim::InputFrame> frames;
    sim::InputFrame previous;
    for (std::uint64_t tick = 0; tick < ticks; ++tick) {
        sim::InputFrame input = ScriptedInput(tick);
        input.pressed = input.held & ~previous.held;
        input.released = previous.held & ~input.held;
        frames.push_back(input);
        previous = input;
    }
    return {.buildVersion = "test", .scene = "test-track", .seed = 8, .racers = {frames}};
}

}
