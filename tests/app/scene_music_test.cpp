#include "app/scene_music.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "audio/ramp_stream.hpp"

namespace rr::app {
namespace {

constexpr std::uint32_t kRateHz = 22050;
constexpr std::uint64_t kTrackFrames = 64;
constexpr std::size_t kRenderFrames = 1024;

class RampSource final : public audio::MusicSource {
public:
    [[nodiscard]] std::string Name() const override { return "ramps"; }

    [[nodiscard]] Result<audio::MusicTrack> Open(
        [[maybe_unused]] std::string_view name) const override {
        return audio::MusicTrack{.stream = std::make_unique<test::RampStream>(kTrackFrames),
                                 .loop = {}};
    }
};

class Jukebox {
public:
    Jukebox() : mixer_(Headless()), music_(mixer_, Sources()) {}

    void Show(const SceneMusic& wanted) { follower_.Update(music_, wanted, ++changes_); }
    void Stay(const SceneMusic& wanted) { follower_.Update(music_, wanted, changes_); }

    void PlayOut() {
        std::array<float, kRenderFrames * 2> output{};
        mixer_.Render(output);
    }

    [[nodiscard]] const audio::MusicPlayer& Music() const { return music_; }

private:
    static audio::Mixer Headless() {
        Result<audio::Mixer> mixer = audio::Mixer::CreateHeadless(kRateHz);
        REQUIRE(mixer.has_value());
        return std::move(mixer).value();
    }

    static std::vector<std::unique_ptr<audio::MusicSource>> Sources() {
        std::vector<std::unique_ptr<audio::MusicSource>> sources;
        sources.push_back(std::make_unique<RampSource>());
        return sources;
    }

    audio::Mixer mixer_;
    audio::MusicPlayer music_;
    SceneMusicFollower follower_;
    std::uint64_t changes_ = 0;
};

constexpr SceneMusic kMenu{.track = "theme", .start = MusicStart::OnEntryIfSilent};
constexpr SceneMusic kAward{
    .track = "1st", .repeat = audio::Repeat::Once, .start = MusicStart::OnEntry};
constexpr SceneMusic kRace{.track = "circuit1"};

}

TEST_CASE("the menu theme waits for an award tune to end and for the menu to open again") {
    Jukebox jukebox;
    jukebox.Show(kAward);
    jukebox.Show(kMenu);
    CHECK(jukebox.Music().Current() == "1st");

    jukebox.PlayOut();
    jukebox.Stay(kMenu);
    CHECK_FALSE(jukebox.Music().IsPlaying());

    jukebox.Show(kMenu);
    CHECK(jukebox.Music().Current() == "theme");
    CHECK(jukebox.Music().IsPlaying());
}

TEST_CASE("leaving a race stops its music, so the menu theme starts") {
    Jukebox jukebox;
    jukebox.Show(kRace);
    CHECK(jukebox.Music().Current() == "circuit1");

    jukebox.Show(kMenu);
    CHECK(jukebox.Music().Current() == "theme");
}

TEST_CASE("a scene that keeps the music leaves the menu theme playing") {
    Jukebox jukebox;
    jukebox.Show(kMenu);
    jukebox.Show({.start = MusicStart::Keep});
    jukebox.Stay({.start = MusicStart::Keep});

    CHECK(jukebox.Music().Current() == "theme");
    CHECK(jukebox.Music().IsPlaying());
}

TEST_CASE("an award tune plays on entry even over the menu theme") {
    Jukebox jukebox;
    jukebox.Show(kMenu);
    jukebox.Show(kAward);

    CHECK(jukebox.Music().Current() == "1st");
}

TEST_CASE("a race asks for its track again each time it's entered") {
    Jukebox jukebox;
    jukebox.Show(kRace);
    jukebox.Show(kAward);
    jukebox.Show(kRace);

    CHECK(jukebox.Music().Current() == "circuit1");
}

}
