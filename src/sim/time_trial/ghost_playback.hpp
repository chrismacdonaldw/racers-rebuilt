#pragma once

#include <optional>

#include "assets/ghost_run.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"

namespace rr::sim {

inline constexpr Milliseconds kGhostSampleInterval{250};

struct GhostFrame {
    Transform pose;
    float chassisAnimationRate = 0.0F;
};

/// The run `time` after the go, or nothing once it has no next sample to blend towards.
[[nodiscard]] std::optional<GhostFrame> GhostFrameAt(const assets::GhostRun& run,
                                                     Milliseconds time);

}
