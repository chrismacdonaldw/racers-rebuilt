#include "app/sound_banks.hpp"

#include <doctest/doctest.h>

#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "../assets/archive_survey.hpp"
#include "assets/track_loader.hpp"

namespace rr::app {
namespace {

audio::Mixer HeadlessMixer() {
    Result<audio::Mixer> mixer = audio::Mixer::CreateHeadless(22050);
    REQUIRE(mixer.has_value());
    return std::move(mixer).value();
}

TEST_CASE("a race track's ambient bank is named after its folder") {
    CHECK(AmbientBankPath("RACEC0R0") == "GAMEDATA/RACEC0R0/LEVC0R0.SBK");
    CHECK(AmbientBankPath("racec2r1") == "GAMEDATA/RACEC2R1/LEVC2R1.SBK");
    CHECK(AmbientBankPath("TEST") == "GAMEDATA/TEST/TEST.SBK");
}

void CheckTrackSounds(audio::Mixer& mixer, SoundBanks& banks, const assets::Vfs& vfs,
                      const std::string& track) {
    const Result<std::span<const audio::SoundId>> ambient =
        banks.Load(mixer, vfs, AmbientBankPath(track));
    const Result<std::vector<std::string>> music = LoadMusicList(vfs, track);
    CHECK_MESSAGE(ambient.has_value(), track);
    CHECK_MESSAGE(music.transform([](const auto& ids) { return ids.size(); }).value_or(0) >= 6,
                  track);
}

TEST_CASE("every retail race track loads its sounds and music list") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    audio::Mixer mixer = HeadlessMixer();
    SoundBanks banks;

    const Result<std::span<const audio::SoundId>> effects =
        banks.Load(mixer, *vfs, "GAMEDATA/COMMON/GENERAL.SBK");
    const Result<std::span<const audio::SoundId>> again =
        banks.Load(mixer, *vfs, "GAMEDATA/COMMON/GENERAL.SBK");

    REQUIRE(effects.has_value());
    CHECK(effects->size() == 80);
    CHECK(again.transform([](std::span<const audio::SoundId> bank) { return bank.data(); }) ==
          effects->data());
    for (const std::string& track : assets::ListRaceTracks(*vfs)) {
        CheckTrackSounds(mixer, banks, *vfs, track);
    }
    CHECK(LoadMusicList(*vfs, "RACEC0R0").value_or(std::vector<std::string>{}).at(1) == "royal");
}

}
}
