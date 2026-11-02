#include "app/menu/option_settings.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "app/audio_settings.hpp"
#include "app/race_settings.hpp"

namespace rr::app {
namespace {

template <settings::SettingType T>
SettingChange ChangeOf(const settings::SettingSpec<T>& spec, T value) {
    return {.key = std::string(spec.key), .value = settings::ToSettingValue(value)};
}

}

int VolumeStep(double volume) {
    const double clamped = std::isnan(volume) ? 0.0 : std::clamp(volume, 0.0, 1.0);
    return static_cast<int>(std::lround(clamped * kMaxVolumeStep));
}

double StepVolume(int step) {
    return static_cast<double>(step) / kMaxVolumeStep;
}

OptionValues ReadOptionValues(const settings::Registry& settings) {
    return {.opponents = settings.Get(kOpponents),
            .versusLaps = settings.Get(kVersusLaps),
            .musicStep = VolumeStep(settings.Get(kMusicVolume)),
            .soundStep = VolumeStep(settings.Get(kEffectsVolume)),
            .isStereo = settings.Get(kStereo)};
}

std::vector<SettingChange> OptionChanges(const OptionValues& before, const OptionValues& after) {
    std::vector<SettingChange> changes;
    if (after.opponents != before.opponents) {
        changes.push_back(ChangeOf(kOpponents, after.opponents));
    }
    if (after.versusLaps != before.versusLaps) {
        changes.push_back(ChangeOf(kVersusLaps, after.versusLaps));
    }
    if (after.musicStep != before.musicStep) {
        changes.push_back(ChangeOf(kMusicVolume, StepVolume(after.musicStep)));
    }
    if (after.soundStep != before.soundStep) {
        changes.push_back(ChangeOf(kEffectsVolume, StepVolume(after.soundStep)));
        changes.push_back(ChangeOf(kVoiceVolume, StepVolume(after.soundStep)));
    }
    if (after.isStereo != before.isStereo) {
        changes.push_back(ChangeOf(kStereo, after.isStereo));
    }
    return changes;
}

}
