#include "app/track_info.hpp"

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <vector>

namespace rr::app {
namespace {

std::string ErrorFor(std::string_view text) {
    const Result<TrackInfo> info = ParseTrackInfo(text, "lavalane", "TRACK.TOML");
    REQUIRE_FALSE(info.has_value());
    return info.error().message;
}

TEST_CASE("track info reads every key") {
    const Result<TrackInfo> info = ParseTrackInfo(
        "name = \"Lava Lane\"\npreview = \"PREVIEW.png\"\nlaps = 5\ngrid = \"c2\"\n"
        "time_trial = true\nmirrored = true\nmusic = [\"lavalane\", \"lavalane2\"]\n",
        "lavalane", "TRACK.TOML");

    REQUIRE(info.has_value());
    CHECK(info->name == "Lava Lane");
    CHECK(info->preview == "GAMEDATA/LAVALANE/PREVIEW.png");
    CHECK(info->lapCount == 5);
    CHECK(info->grid == "c2");
    CHECK(info->hasTimeTrial);
    CHECK(info->hasMirrored);
    CHECK(info->music == std::vector<std::string>{"lavalane", "lavalane2"});
}

TEST_CASE("every track info key is optional") {
    const Result<TrackInfo> info = ParseTrackInfo("", "lavalane", "TRACK.TOML");

    REQUIRE(info.has_value());
    CHECK(info->name.empty());
    CHECK_FALSE(info->preview.has_value());
    CHECK_FALSE(info->lapCount.has_value());
    CHECK_FALSE(info->grid.has_value());
    CHECK_FALSE(info->hasTimeTrial);
    CHECK_FALSE(info->hasMirrored);
    CHECK(info->music.empty());
}

TEST_CASE("a mistyped or out of range track info key is an error") {
    CHECK(ErrorFor("name = 3\n") == "TRACK.TOML: name must be text");
    CHECK(ErrorFor("laps = 0\n") == "TRACK.TOML: laps must be 1 to 9");
    CHECK(ErrorFor("laps = 2.5\n") == "TRACK.TOML: laps must be a whole number");
    CHECK(ErrorFor("mirrored = \"yes\"\n") == "TRACK.TOML: mirrored must be true or false");
    CHECK(ErrorFor("music = [\"a\", 2]\n") == "TRACK.TOML: music must be a list of music ids");
    CHECK(ErrorFor("name = \n").starts_with("TRACK.TOML line 1:"));
}

TEST_CASE("track music takes the race slots and keeps the intro, lose and win music") {
    const std::vector<std::string> retail{"intro", "race", "lose", "win", "race2"};

    CHECK(WithTrackMusic(retail, {}) == retail);
    CHECK(WithTrackMusic(retail, {"lava", "lava2"}) ==
          std::vector<std::string>{"intro", "lava", "lose", "win", "lava2"});
    CHECK(WithTrackMusic({}, {"lava"}) == std::vector<std::string>{"", "lava", "", ""});
}

}
}
