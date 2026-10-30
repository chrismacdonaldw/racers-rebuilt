#include "app/race_music.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace rr::app {
namespace {

constexpr std::array kGo{sim::SoundEvent{.sound = sim::RaceSound::Go}};
constexpr std::array kPlayerFinish{sim::SoundEvent{.sound = sim::RaceSound::Finish}};
constexpr std::array kOtherFinish{sim::SoundEvent{.sound = sim::RaceSound::Finish, .racer = 1}};

std::vector<std::string> SevenTracks() {
    return {"start", "royal", "lose", "win", "knight", "moat", "armor"};
}

std::string RaceTrackFor(std::vector<std::string> list, std::uint32_t pick) {
    RaceMusic music(std::move(list), pick);
    music.Hear(kGo, 0, std::nullopt);
    return std::string(music.Current().track);
}

TEST_CASE("a race plays its intro once, then loops race music from go") {
    RaceMusic music(SevenTracks(), 0);

    CHECK(music.Current().track == "start");
    CHECK(music.Current().repeat == audio::Repeat::Once);
    music.Hear(kGo, 0, std::nullopt);
    CHECK(music.Current().track == "royal");
    CHECK(music.Current().repeat == audio::Repeat::Loop);
}

TEST_CASE("the race music pick spreads over tracks 1, 4, 5 and 6") {
    CHECK(RaceTrackFor(SevenTracks(), 0) == "royal");
    CHECK(RaceTrackFor(SevenTracks(), 1) == "knight");
    CHECK(RaceTrackFor(SevenTracks(), 2) == "moat");
    CHECK(RaceTrackFor(SevenTracks(), 3) == "armor");
    CHECK(RaceTrackFor(SevenTracks(), 4) == "royal");
    CHECK(RaceTrackFor({"start", "royal", "lose", "win"}, 3) == "royal");
}

TEST_CASE("the player's finish plays the win or lose track once") {
    RaceMusic first(SevenTracks(), 0);
    RaceMusic second(SevenTracks(), 0);

    first.Hear(kOtherFinish, 0, std::nullopt);
    CHECK(first.Current().track == "start");
    first.Hear(kPlayerFinish, 0, 1);
    second.Hear(kPlayerFinish, 0, 2);

    CHECK(first.Current().track == "win");
    CHECK(first.Current().repeat == audio::Repeat::Once);
    CHECK(second.Current().track == "lose");
}

TEST_CASE("a race without a music list is silent") {
    RaceMusic music({}, 0);
    music.Hear(kGo, 0, std::nullopt);

    CHECK(music.Current().track.empty());
}

}
}
