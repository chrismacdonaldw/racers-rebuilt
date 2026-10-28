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

TEST_CASE("the command line takes an image dump folder") {
    constexpr std::array<std::string_view, 1> kArgs{"--dump-images=dumps/images"};

    Result<CommandLine> commandLine = ParseCommandLine(kArgs);

    REQUIRE(commandLine.has_value());
    CHECK(commandLine->imageDumpFolder == "dumps/images");
    CHECK_FALSE(commandLine->gamePath.has_value());
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
