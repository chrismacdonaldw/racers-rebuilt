#include "settings/command_line.hpp"

#include <doctest/doctest.h>

#include <array>

namespace rr::settings {
namespace {

TEST_CASE("the command line takes a game path and setting overrides") {
    constexpr std::array<std::string_view, 4> kArgs{"--game-path", "D:/LEGO Racers",
                                                    "--set=video.fps_cap=240", "--set=a.b="};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->gamePath == "D:/LEGO Racers");
    REQUIRE(commandLine->overrides.size() == 2);
    CHECK(commandLine->overrides[0].key == "video.fps_cap");
    CHECK(commandLine->overrides[0].value == "240");
    CHECK(commandLine->overrides[1].value.empty());
}

TEST_CASE("--novideo takes no value") {
    constexpr std::array<std::string_view, 3> kArgs{"--novideo", "--track", "RACEC0R0"};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->noVideo);
    CHECK(commandLine->track == "RACEC0R0");
}

TEST_CASE("the command line takes an image dump folder") {
    constexpr std::array<std::string_view, 1> kArgs{"--dump-images=dumps/images"};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->imageDumpFolder == "dumps/images");
    CHECK_FALSE(commandLine->gamePath.has_value());
}

TEST_CASE("the command line takes a track to view") {
    constexpr std::array<std::string_view, 2> kArgs{"--track", "RACEC0R0"};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->track == "RACEC0R0");
}

TEST_CASE("the command line takes a track to race on") {
    constexpr std::array<std::string_view, 1> kArgs{"--race=RACEC1R2"};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->race == "RACEC1R2");
    CHECK_FALSE(commandLine->track.has_value());
}

TEST_CASE("the command line takes a cutscene to play") {
    constexpr std::array<std::string_view, 2> kArgs{"--cutscene", "CIRCUIT1/CIR1-01"};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->cutscene == "CIRCUIT1/CIR1-01");
}

TEST_CASE("the command line takes a racer whose car to show") {
    constexpr std::array<std::string_view, 1> kArgs{"--show-car=JOAN OF KART"};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->shownCar == "JOAN OF KART");
}

TEST_CASE("the command line takes the racer the player races") {
    constexpr std::array<std::string_view, 2> kArgs{"--racer", "JOAN OF KART"};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->racer == "JOAN OF KART");
}

TEST_CASE("the command line takes a player count") {
    constexpr std::array<std::string_view, 4> kArgs{"--race", "RACEC0R0", "--players", "2"};
    constexpr std::array<std::string_view, 1> kNone{"--players=0"};
    constexpr std::array<std::string_view, 1> kWord{"--players=two"};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->players == 2U);
    CHECK_FALSE(ParseCommandLine(kNone).has_value());
    CHECK_FALSE(ParseCommandLine(kWord).has_value());
}

TEST_CASE("the command line takes a replay to record or play") {
    constexpr std::array<std::string_view, 3> kArgs{"--record", "a.rrr", "--replay=b.rrr"};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->recordFile == "a.rrr");
    CHECK(commandLine->replayFile == "b.rrr");
}

TEST_CASE("the command line takes an original save to import") {
    constexpr std::array<std::string_view, 1> kArgs{"--import-save=Save/0/LEGORac1"};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->importSaveFile == "Save/0/LEGORac1");
}

TEST_CASE("the command line rejects unknown options and missing values") {
    constexpr std::array<std::string_view, 1> kUnknown{"--fullscreen"};
    constexpr std::array<std::string_view, 1> kMissing{"--game-path"};
    constexpr std::array<std::string_view, 2> kMalformed{"--set", "fps"};

    CHECK_FALSE(ParseCommandLine(kUnknown).has_value());
    CHECK_FALSE(ParseCommandLine(kMissing).has_value());
    CHECK_FALSE(ParseCommandLine(kMalformed).has_value());
}

TEST_CASE("an override for an unknown setting names it") {
    Registry registry;
    const std::array overrides{SettingOverride{.key = "video.missing", .value = "1"}};

    Result<void> applied = ApplyOverrides(registry, overrides);

    REQUIRE_FALSE(applied.has_value());
    CHECK(applied.error().message.find("video.missing") != std::string::npos);
}

}
}
