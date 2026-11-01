#pragma once

#include <array>

namespace rr::sim {

struct FrameSpan {
    float after = 0.0F;
    float before = 0.0F;
};

struct StrikeWindows {
    std::array<FrameSpan, 2> strike;
    std::array<FrameSpan, 2> rearm;
};

/// Strikes and disarms inside `strike`, re-arms inside `rearm`, and returns whether it struck.
[[nodiscard]] bool Swing(const StrikeWindows& windows, float frame, bool& isArmed);

}
