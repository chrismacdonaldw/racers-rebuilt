#include "app/video_settings.hpp"

namespace rr::app {

void DeclareVideoSettings(settings::Registry& registry) {
    registry.Declare(kVsyncSetting);
    registry.Declare(kFpsCapSetting);
}

platform::PacingConfig PacingFromSettings(const settings::Registry& registry) {
    if (registry.Get(kVsyncSetting)) {
        return {.mode = platform::PresentMode::Vsync};
    }
    const int fpsCap = registry.Get(kFpsCapSetting);
    if (fpsCap == 0) {
        return {.mode = platform::PresentMode::Uncapped};
    }
    return {.mode = platform::PresentMode::Capped, .fpsCap = fpsCap};
}

void StorePacing(settings::Registry& registry, const platform::PacingConfig& pacing) {
    if (pacing.mode != platform::PresentMode::Vsync) {
        registry.Set(kFpsCapSetting,
                     pacing.mode == platform::PresentMode::Uncapped ? 0 : pacing.fpsCap);
    }
    registry.Set(kVsyncSetting, pacing.mode == platform::PresentMode::Vsync);
}

}
