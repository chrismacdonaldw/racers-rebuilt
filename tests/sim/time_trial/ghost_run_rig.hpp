#pragma once

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>

#include "assets/ghost_run.hpp"
#include "core/transform.hpp"
#include "sim/time_trial/ghost_playback.hpp"

namespace rr::test {

/// A run that moves one unit along x per sample, facing +X, with `samples` samples.
[[nodiscard]] inline assets::GhostRun StraightGhost(int samples,
                                                    const assets::GhostLapTimes& laps) {
    assets::GhostRun run{.laps = laps,
                         .start = {.position = {.x = 0.0F}, .rotation = Quaternion{}}};
    for (int i = 0; i < samples; ++i) {
        run.samples.push_back(
            {.position = {static_cast<std::int16_t>(i * 32), 0, 0}, .rotation = {0, 0, 0, 127}});
    }
    return run;
}

[[nodiscard]] constexpr assets::GhostLapTimes Laps(std::int64_t first, std::int64_t second,
                                                   std::int64_t third) {
    return {Milliseconds{first}, Milliseconds{second}, Milliseconds{third}};
}

[[nodiscard]] constexpr Transform At(float x) {
    return {.position = {.x = x}, .rotation = Quaternion{}};
}

[[nodiscard]] inline sim::GhostFrame FrameOf(const std::optional<sim::GhostFrame>& frame) {
    REQUIRE(frame.has_value());
    return frame.value_or(sim::GhostFrame{});
}

}
