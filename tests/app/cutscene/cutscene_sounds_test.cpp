#include "app/cutscene/cutscene_sounds.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../audio/alp_builder.hpp"
#include "app/cutscene/cutscene_effect_player.hpp"
#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

constexpr std::uint32_t kRateHz = 22050;
constexpr std::size_t kFramesPerRender = 1024;
constexpr std::array<std::uint8_t, 256> kPayload = [] {
    std::array<std::uint8_t, 256> payload{};
    payload.fill(0x77);
    return payload;
}();

std::span<const std::byte> Alp() {
    static const std::vector<std::byte> alp = test::BuildAlp(1, kRateHz, kPayload);
    return alp;
}

audio::Mixer HeadlessMixer() {
    Result<audio::Mixer> mixer = audio::Mixer::CreateHeadless(kRateHz);
    REQUIRE(mixer.has_value());
    return std::move(mixer).value();
}

class Speakers {
public:
    explicit Speakers(std::vector<assets::CutsceneSound> sounds)
        : mixer_(HeadlessMixer()),
          sound_(mixer_.AddSound(assets::FileData(Alp())).value_or(audio::SoundId{})),
          sounds_(std::move(sounds)),
          player_(mixer_, sounds_,
                  std::vector<std::optional<audio::SoundId>>(sounds_.size(), sound_)) {}

    Speakers(std::vector<assets::CutsceneSound> sounds,
             std::vector<std::optional<audio::SoundId>> ids)
        : mixer_(HeadlessMixer()),
          sound_(mixer_.AddSound(assets::FileData(Alp())).value_or(audio::SoundId{})),
          sounds_(std::move(sounds)),
          player_(mixer_, sounds_, std::move(ids)) {}

    bool IsSilent() {
        mixer_.Render(output_);
        return std::ranges::all_of(output_, [](float sample) { return sample == 0.0F; });
    }

    CutsceneSounds& Player() { return player_; }

private:
    audio::Mixer mixer_;
    audio::SoundId sound_;
    std::vector<assets::CutsceneSound> sounds_;
    CutsceneSounds player_;
    std::array<float, kFramesPerRender * 2> output_{};
};

TEST_CASE("a cutscene sound plays once, and a second start doesn't add another") {
    Speakers speakers({{.name = "peg"}});

    speakers.Player().Start(0);
    speakers.Player().Start(0);
    speakers.Player().Stop(0);
    CHECK(speakers.IsSilent());

    speakers.Player().Start(0);
    CHECK_FALSE(speakers.IsSilent());
    CHECK(speakers.IsSilent());
    speakers.Player().Start(0);
    CHECK_FALSE(speakers.IsSilent());
}

TEST_CASE("a looping cutscene sound plays until it stops") {
    Speakers speakers({{.name = "ocean", .isLooping = true}, {.name = "bird"}});

    speakers.Player().Start(0);
    for (int render = 0; render < 3; ++render) {
        CHECK_FALSE(speakers.IsSilent());
    }
    speakers.Player().Start(1);
    speakers.Player().StopAll();
    CHECK(speakers.IsSilent());
}

TEST_CASE("a sound whose bank is missing stays silent") {
    Speakers speakers({{.name = "cheer"}}, {std::nullopt});

    speakers.Player().Start(0);

    CHECK(speakers.IsSilent());
}

struct FoundSounds {
    std::ptrdiff_t found = 0;
    std::ptrdiff_t total = 0;
};

Result<FoundSounds> FindSounds(audio::Mixer& mixer, SoundBanks& banks, const assets::Vfs& vfs,
                               std::string_view path) {
    const std::string folder = std::format("MENUDATA/{}", path.substr(0, path.find('/')));
    const Result<assets::CutsceneEffectsAsset> effects =
        vfs.Read(std::format("MENUDATA/{}.CEB", path)).and_then([](const assets::FileData& data) {
            return assets::ParseCutsceneEffects(data.Bytes());
        });
    return effects
        .and_then([&](const assets::CutsceneEffectsAsset& read) {
            return LoadCutsceneSounds(mixer, banks, vfs, folder, read);
        })
        .transform([&](const std::vector<std::optional<audio::SoundId>>& sounds) {
            return FoundSounds{.found = std::ranges::count_if(
                                   sounds, [](const auto& id) { return id.has_value(); }),
                               .total = std::ssize(effects->sounds)};
        });
}

TEST_CASE("every retail cutscene's sounds are in its banks, except WINCAR's") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    audio::Mixer mixer = HeadlessMixer();
    SoundBanks banks;
    for (const std::string_view path :
         {"LEGAL/SPLASH1", "CIRCUIT1/CIR1-01", "CIRCUIT2/TEST", "CIRCUIT3/CIR3-01",
          "CIRCUIT4/CIR4-01", "CIRCUIT5/CIR5-01", "CIRCUIT6/TEST", "CIRCUIT7/FR",
          "C_AWARD1/1STPLACE", "C_AWARD2/TWO", "C_AWARD3/3-01", "C_AWARD4/LOSE", "WINCAR/WINCAR",
          "WINRRCAR/END", "WINVVCAR/VVWIN", "CREDITS/CREDITS", "SINGRACE/PST"}) {
        CAPTURE(path);
        const Result<FoundSounds> sounds = FindSounds(mixer, banks, *vfs, path);
        REQUIRE_MESSAGE(sounds.has_value(), sounds.error().message);
        CHECK(sounds->found == (path == "WINCAR/WINCAR" ? 0 : sounds->total));
    }
}

TEST_CASE("a transform's links start and stop overlay effects") {
    const assets::CutsceneFrame frame{.transforms = {{.name = "line1"}}};
    const assets::CutsceneEffectsAsset effects{
        .images = {{.name = "logo"}},
        .links = {{.transform = "line1", .kind = assets::EffectKind::Image},
                  {.transform = "line1",
                   .onTransformEnd = true,
                   .kind = assets::EffectKind::Image,
                   .starts = false}}};
    audio::Mixer mixer = HeadlessMixer();
    CutsceneEffectPlayer player(
        frame, effects, CutsceneSounds(mixer, {}, {}),
        CutsceneOverlay(
            effects, {.images = {{.texture = render::TextureId{2}, .widthPx = 4, .heightPx = 4}}}));
    const auto drawn = [&] {
        std::vector<render::SpriteDraw> out;
        player.Draw({.width = 640.0F, .height = 480.0F}, out);
        return out.size();
    };
    const std::array begins{CutsceneEventChange{
        .event = {.kind = CutsceneEventKind::Transform, .index = 0}, .begins = true}};
    const std::array ends{CutsceneEventChange{
        .event = {.kind = CutsceneEventKind::Transform, .index = 0}, .begins = false}};

    player.Advance(begins, Milliseconds{10});
    CHECK(drawn() == 1);
    player.Advance(ends, Milliseconds{10});
    CHECK(drawn() == 0);
    player.Advance(begins, Milliseconds{10});
    player.StopAll();
    CHECK(drawn() == 0);
}

}
}
