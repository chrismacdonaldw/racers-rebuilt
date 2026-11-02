#include "app/menu/option_settings.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "app/audio_settings.hpp"
#include "app/menu/options_look.hpp"
#include "app/race_settings.hpp"

namespace rr::app {
namespace {

settings::Registry Declared() {
    settings::Registry registry;
    DeclareAudioSettings(registry);
    DeclareRaceSettings(registry);
    return registry;
}

std::vector<std::string> KeysOf(const std::vector<SettingChange>& changes) {
    std::vector<std::string> keys;
    keys.reserve(changes.size());
    for (const SettingChange& change : changes) {
        keys.push_back(change.key);
    }
    return keys;
}

TEST_CASE("option values read the settings, volumes rounded to the nearest of 20 steps") {
    settings::Registry registry = Declared();
    registry.Set(kMusicVolume, 0.33);
    registry.Set(kEffectsVolume, 0.5);
    registry.Set(kStereo, false);
    registry.Set(kOpponents, 2);

    CHECK(ReadOptionValues(registry) ==
          OptionValues{.opponents = 2, .musicStep = 7, .soundStep = 10, .isStereo = false});
}

TEST_CASE("a volume outside 0 to 1 or not a number lands on the nearest end step") {
    CHECK(VolumeStep(std::numeric_limits<double>::quiet_NaN()) == 0);
    CHECK(VolumeStep(-0.5) == 0);
    CHECK(VolumeStep(4.0) == kMaxVolumeStep);
}

TEST_CASE("changed option values become setting changes, the sound step setting both buses") {
    const OptionValues before;
    const OptionValues after{.versusLaps = 5, .soundStep = 4};

    const std::vector<SettingChange> changes = OptionChanges(before, after);

    CHECK(KeysOf(changes) == std::vector<std::string>{std::string(kVersusLaps.key),
                                                      std::string(kEffectsVolume.key),
                                                      std::string(kVoiceVolume.key)});
    CHECK(changes[0].value == settings::SettingValue{std::int64_t{5}});
    CHECK(changes[1].value == settings::SettingValue{0.2});
}

TEST_CASE("unchanged option values change no settings") {
    CHECK(OptionChanges(OptionValues{}, OptionValues{}).empty());
}

TEST_CASE("a slider's thumb runs between its step buttons, centred on its step") {
    OptionsLook look;
    look.stepDown = {.widthPx = 64, .heightPx = 64};
    look.stepUp = {.widthPx = 64, .heightPx = 64};
    look.rail = {.widthPx = 16, .heightPx = 8};
    look.thumb = {.widthPx = 12, .heightPx = 24};
    const render::Rect bounds{.x = 20.0F, .y = 98.0F, .width = 301.0F, .height = 65.0F};

    const SliderParts lowest = LayOutSlider(bounds, look, 0);
    const SliderParts highest = LayOutSlider(bounds, look, kMaxVolumeStep);

    CHECK(lowest.stepUp.x == doctest::Approx(257.0F));
    CHECK(lowest.rail.x == doctest::Approx(85.0F));
    CHECK(lowest.rail.width == doctest::Approx(171.0F));
    CHECK(lowest.rail.y == doctest::Approx(126.5F));
    CHECK(lowest.thumb.x == doctest::Approx(84.0F));
    CHECK(highest.thumb.x + highest.thumb.width == doctest::Approx(257.0F));
    CHECK(highest.thumb.y == doctest::Approx(118.5F));
}

}
}
