#pragma once

#include "platform/frame_pacer.hpp"
#include "render/renderer.hpp"
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

inline constexpr settings::SettingSpec<double> kResolutionScaleSetting{
    .key = "video.resolution_scale",
    .defaultValue = 1.0,
    .range = settings::SettingRange<double>{.min = render::kMinResolutionScale,
                                            .max = render::kMaxResolutionScale},
    .label = "Resolution scale",
    .category = "Video"};

inline constexpr settings::SettingSpec<bool> kStretchUiSetting{
    .key = "video.stretch_ui", .defaultValue = false, .label = "Stretch UI", .category = "Video"};

inline constexpr settings::SettingSpec<bool> kWidescreenSetting{
    .key = "video.widescreen", .defaultValue = false, .label = "Widescreen", .category = "Video"};

/// Retail textures' mip levels and colour-key edges filled from neighbours, unlike the original.
inline constexpr settings::SettingSpec<bool> kSmoothTexturesSetting{.key = "video.smooth_textures",
                                                                    .defaultValue = true,
                                                                    .label = "Smooth textures",
                                                                    .category = "Video"};

inline constexpr settings::SettingSpec<bool> kAnisotropicFilteringSetting{
    .key = "video.anisotropic_filtering",
    .defaultValue = false,
    .label = "Anisotropic filtering",
    .category = "Video"};

/// Draws cars past the original's 500 units from the camera.
inline constexpr settings::SettingSpec<bool> kFarCarsSetting{
    .key = "video.far_cars", .defaultValue = true, .label = "Draw far cars", .category = "Video"};

/// Draws track models past the distance each one's world file gives, unlike the original.
inline constexpr settings::SettingSpec<bool> kFarScenerySetting{.key = "video.far_scenery",
                                                                .defaultValue = true,
                                                                .label = "Draw far scenery",
                                                                .category = "Video"};

void DeclareVideoSettings(settings::Registry& registry);

[[nodiscard]] platform::PacingConfig PacingFromSettings(const settings::Registry& registry);

void StorePacing(settings::Registry& registry, const platform::PacingConfig& pacing);

void ApplyRendererSettings(const settings::Registry& registry, render::Renderer& renderer);

}
