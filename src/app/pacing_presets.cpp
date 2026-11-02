#include "app/pacing_presets.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::array kPacingPresets{
    platform::PacingConfig{.mode = platform::PresentMode::Vsync},
    platform::PacingConfig{.mode = platform::PresentMode::Capped, .fpsCap = 30},
    platform::PacingConfig{.mode = platform::PresentMode::Capped, .fpsCap = 60},
    platform::PacingConfig{.mode = platform::PresentMode::Capped, .fpsCap = 144},
    platform::PacingConfig{.mode = platform::PresentMode::Uncapped},
};

}

bool IsSamePacing(const platform::PacingConfig& a, const platform::PacingConfig& b) {
    return a.mode == b.mode && (a.mode != platform::PresentMode::Capped || a.fpsCap == b.fpsCap);
}

platform::PacingConfig NextPacingPreset(const platform::PacingConfig& current) {
    const auto match = std::ranges::find_if(
        kPacingPresets,
        [&](const platform::PacingConfig& preset) { return IsSamePacing(preset, current); });
    const std::size_t next = match == kPacingPresets.end()
                                 ? 0
                                 : (static_cast<std::size_t>(match - kPacingPresets.begin()) + 1) %
                                       kPacingPresets.size();
    return kPacingPresets.at(next);
}

void LogPacing(const platform::PacingConfig& pacing) {
    switch (pacing.mode) {
        case platform::PresentMode::Vsync:
            log::Info("pacing: VSync");
            break;
        case platform::PresentMode::Capped:
            log::Info("pacing: capped at {} FPS", pacing.fpsCap);
            break;
        case platform::PresentMode::Uncapped:
            log::Info("pacing: uncapped");
            break;
    }
}

Result<platform::FramePacer> CreatePacer(const platform::PacingConfig& pacing) {
    Result<platform::FrameTimer> timer = platform::FrameTimer::Create();
    if (!timer) {
        return std::unexpected(std::move(timer.error()));
    }
    LogPacing(pacing);
    return platform::FramePacer(std::move(*timer), pacing);
}

}
