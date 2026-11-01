#include "sim/hazard/swing_strike.hpp"

#include <algorithm>

namespace rr::sim {
namespace {

bool IsInside(const std::array<FrameSpan, 2>& spans, float frame) {
    return std::ranges::any_of(
        spans, [&](const FrameSpan& span) { return span.after < frame && frame < span.before; });
}

}

bool Swing(const StrikeWindows& windows, float frame, bool& isArmed) {
    if (isArmed && IsInside(windows.strike, frame)) {
        isArmed = false;
        return true;
    }
    if (!isArmed && IsInside(windows.rearm, frame)) {
        isArmed = true;
    }
    return false;
}

}
