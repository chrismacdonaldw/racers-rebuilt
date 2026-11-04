#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/application.hpp"
#include "app/pacing_presets.hpp"
#include "app/startup_settings.hpp"
#include "app/video_settings.hpp"
#include "core/log.hpp"

namespace rr::app {

void Application::StoreSceneSettings() {
    Scene* scene = frameLoop_.Scenes().Top();
    if (scene == nullptr) {
        return;
    }
    std::vector<std::string> keys;
    for (SettingChange& change : scene->TakeSettingChanges()) {
        const Result<void> stored = settings_.Assign(change.key, std::move(change.value));
        if (!stored) {
            log::Error("{}", stored.error().message);
            continue;
        }
        keys.push_back(std::move(change.key));
    }
    Persist(keys);
}

void Application::CyclePacing() {
    StorePacing(settings_, NextPacingPreset(PacingFromSettings(settings_)));
    const std::array<std::string_view, 2> keys{kVsyncSetting.key, kFpsCapSetting.key};
    PersistSettings(settingsFile_, settings_, keys);
}

void Application::ToggleFullscreen() {
    settings_.Set(kFullscreenSetting, !settings_.Get(kFullscreenSetting));
    const std::array<std::string_view, 1> keys{kFullscreenSetting.key};
    PersistSettings(settingsFile_, settings_, keys);
}

void Application::Persist(const std::vector<std::string>& keys) {
    if (!keys.empty()) {
        const std::vector<std::string_view> views(keys.begin(), keys.end());
        PersistSettings(settingsFile_, settings_, views);
    }
}

void Application::ApplyVideo() {
    ApplyRendererSettings(settings_, renderer_);
    if (Result<void> fullscreen = window_.SetFullscreen(settings_.Get(kFullscreenSetting));
        !fullscreen) {
        log::Error("{}", fullscreen.error().message);
    }
    const platform::PacingConfig pacing = PacingFromSettings(settings_);
    if (IsSamePacing(pacing, pacing_)) {
        return;
    }
    Result<void> changed = renderer_.SetPresentMode(pacing.mode);
    if (!changed) {
        log::Error("{}", changed.error().message);
        return;
    }
    pacer_.SetConfig(pacing);
    pacing_ = pacing;
    LogPacing(pacing);
}

}
