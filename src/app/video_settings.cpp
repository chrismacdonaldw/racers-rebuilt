#include "app/video_settings.hpp"

namespace rr::app {

void DeclareVideoSettings(settings::Registry& registry) {
    registry.Declare(kVsyncSetting);
    registry.Declare(kFpsCapSetting);
    registry.Declare(kResolutionScaleSetting);
    registry.Declare(kStretchUiSetting);
    registry.Declare(kWidescreenSetting);
    registry.Declare(kSmoothTexturesSetting);
    registry.Declare(kFarCarsSetting);
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

void ApplyRendererSettings(const settings::Registry& registry, render::Renderer& renderer) {
    renderer.SetResolutionScale(static_cast<float>(registry.Get(kResolutionScaleSetting)));
    renderer.SetUiFit(registry.Get(kStretchUiSetting) ? render::UiFit::Stretched
                                                      : render::UiFit::Centered);
    renderer.SetSceneFit(registry.Get(kWidescreenSetting) ? render::SceneFit::Window
                                                          : render::SceneFit::Canvas);
    renderer.SetSmoothTextures(registry.Get(kSmoothTexturesSetting));
}

}
