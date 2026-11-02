#pragma once

#include "core/result.hpp"
#include "platform/frame_pacer.hpp"

namespace rr::app {

[[nodiscard]] bool IsSamePacing(const platform::PacingConfig& a, const platform::PacingConfig& b);

/// Cycles VSync, 30, 60 and 144 FPS caps, then uncapped. Pacing off that list goes to VSync.
[[nodiscard]] platform::PacingConfig NextPacingPreset(const platform::PacingConfig& current);

void LogPacing(const platform::PacingConfig& pacing);

[[nodiscard]] Result<platform::FramePacer> CreatePacer(const platform::PacingConfig& pacing);

}
