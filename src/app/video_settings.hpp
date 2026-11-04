#pragma once

#include <array>
#include <optional>
#include <string_view>

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

inline constexpr settings::SettingSpec<bool> kFullscreenSetting{
    .key = "video.fullscreen", .defaultValue = false, .label = "Fullscreen", .category = "Video"};

inline constexpr settings::SettingSpec<bool> kStretchUiSetting{
    .key = "video.stretch_ui", .defaultValue = false, .label = "Stretch UI", .category = "Video"};

inline constexpr settings::SettingSpec<bool> kWidescreenSetting{
    .key = "video.widescreen", .defaultValue = false, .label = "Widescreen", .category = "Video"};

inline constexpr std::array<std::string_view, 3> kHudWidthChoices{"Window", "21:9", "16:9"};

inline constexpr settings::SettingSpec<std::string_view> kHudWidthSetting{
    .key = "video.hud_width",
    .defaultValue = kHudWidthChoices[0],
    .label = "HUD width",
    .category = "Video",
    .choices = kHudWidthChoices,
    .tooltip = "How far out to the window's sides the race HUD reaches.",
    .requirement = settings::SettingRequirement{.key = kWidescreenSetting.key, .choice = "true"}};

inline constexpr settings::SettingSpec<bool> kWidescreenCutscenesSetting{
    .key = "video.widescreen_cutscenes",
    .defaultValue = false,
    .label = "Widescreen cutscenes",
    .category = "Video",
    .requirement = settings::SettingRequirement{.key = kWidescreenSetting.key, .choice = "true"}};

/// Retail textures' mip levels and colour-key edges filled from neighbours, unlike the original.
inline constexpr settings::SettingSpec<bool> kSmoothTexturesSetting{.key = "video.smooth_textures",
                                                                    .defaultValue = false,
                                                                    .label = "Smooth textures",
                                                                    .category = "Video"};

inline constexpr settings::SettingSpec<bool> kAnisotropicFilteringSetting{
    .key = "video.anisotropic_filtering",
    .defaultValue = false,
    .label = "Anisotropic filtering",
    .category = "Video"};

/// Draws cars past the original's 500 units from the camera.
inline constexpr settings::SettingSpec<bool> kFarCarsSetting{
    .key = "video.far_cars", .defaultValue = false, .label = "Draw far cars", .category = "Video"};

/// Draws track models past the distance each one's world file gives, unlike the original.
inline constexpr settings::SettingSpec<bool> kFarScenerySetting{.key = "video.far_scenery",
                                                                .defaultValue = false,
                                                                .label = "Draw far scenery",
                                                                .category = "Video"};

/// Draws the player's own car in the cockpit view, which the original leaves out while racing.
inline constexpr settings::SettingSpec<bool> kCockpitCarSetting{.key = "video.cockpit_car",
                                                                .defaultValue = false,
                                                                .label = "Car in cockpit view",
                                                                .category = "Video"};

void DeclareVideoSettings(settings::Registry& registry);

[[nodiscard]] platform::PacingConfig PacingFromSettings(const settings::Registry& registry);

void StorePacing(settings::Registry& registry, const platform::PacingConfig& pacing);

/// Width over height. None reaches the window's sides.
[[nodiscard]] std::optional<float> HudAspect(std::string_view hudWidth);

void ApplyRendererSettings(const settings::Registry& registry, render::Renderer& renderer);

}
