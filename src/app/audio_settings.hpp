#pragma once

#include <string_view>

#include "audio/mixer.hpp"
#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"

namespace rr::app {

inline constexpr settings::SettingRange<double> kVolumeRange{.min = 0.0, .max = 1.0};

inline constexpr settings::SettingSpec<double> kMusicVolume{.key = "audio.music_volume",
                                                            .defaultValue = 1.0,
                                                            .range = kVolumeRange,
                                                            .label = "Music volume",
                                                            .category = "Audio"};

inline constexpr settings::SettingSpec<double> kEffectsVolume{.key = "audio.effects_volume",
                                                              .defaultValue = 1.0,
                                                              .range = kVolumeRange,
                                                              .label = "Effects volume",
                                                              .category = "Audio"};

inline constexpr settings::SettingSpec<double> kVoiceVolume{.key = "audio.voice_volume",
                                                            .defaultValue = 1.0,
                                                            .range = kVolumeRange,
                                                            .label = "Voice volume",
                                                            .category = "Audio"};

/// A game path such as `GAMEDATA/COMMON/321.PCM` or `THEME.TUN`, played once audio starts.
inline constexpr settings::SettingSpec<std::string_view> kDevPlaySound{
    .key = "dev.play_sound", .defaultValue = "", .label = "Play sound", .category = "Developer"};

void DeclareAudioSettings(settings::Registry& registry);

void ApplyVolumes(const settings::Registry& registry, audio::Mixer& mixer);

}
