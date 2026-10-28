#pragma once

#include "platform/frame_pacer.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kVsyncSetting{
    .key = "video.vsync", .defaultValue = true, .label = "VSync", .category = "Video"};

/// Applies only with VSync off. 0 removes the cap.
inline constexpr settings::SettingSpec<int> kFpsCapSetting{
    .key = "video.fps_cap",
    .defaultValue = 144,
    .range = settings::SettingRange<int>{.min = 0, .max = platform::kMaxFpsCap},
    .label = "Frame rate cap",
    .category = "Video"};

void DeclareVideoSettings(settings::Registry& registry);

[[nodiscard]] platform::PacingConfig PacingFromSettings(const settings::Registry& registry);

void StorePacing(settings::Registry& registry, const platform::PacingConfig& pacing);

}
