#include "app/intro/intro_videos.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <vector>

#include "app/game_settings.hpp"
#include "app/startup_scene.hpp"
#include "settings/command_line.hpp"
#include "settings/registry.hpp"

namespace rr::app {
namespace {

std::vector<std::string> IntroSceneChain() {
    std::vector<std::string> chain{FirstStartupScene(true)};
    while (const std::optional<std::size_t> video = ParseIntroVideoScene(chain.back())) {
        chain.push_back(SceneAfterIntroVideo(*video));
    }
    return chain;
}

std::optional<std::string> StartupWith(bool playsVideos, bool noVideo) {
    settings::Registry registry;
    DeclareGameSettings(registry);
    REQUIRE(registry.AssignText(kIntroVideosSetting.key, playsVideos ? "true" : "false"));
    return StartupScene({.noVideo = noVideo}, registry).value_or(std::nullopt);
}

TEST_CASE("the intro videos play in the original's order, then the legal screen") {
    CHECK(IntroSceneChain() ==
          std::vector<std::string>{"intro:0", "intro:1", "intro:2", "screen:legal"});
    CHECK_FALSE(kIntroVideos[0].isSkippable);
    CHECK(kIntroVideos[1].isSkippable);
    CHECK(kIntroVideos[2].isSkippable);
}

TEST_CASE("an intro video scene name must name one of the videos") {
    CHECK(ParseIntroVideoScene("intro:2") == 2);
    CHECK_FALSE(ParseIntroVideoScene("intro:3").has_value());
    CHECK_FALSE(ParseIntroVideoScene("intro:").has_value());
    CHECK_FALSE(ParseIntroVideoScene("intro:1x").has_value());
    CHECK_FALSE(ParseIntroVideoScene("screen:legal").has_value());
}

TEST_CASE("startup opens the intro videos unless the setting or --novideo turns them off") {
    CHECK(StartupWith(true, false) == "intro:0");
    CHECK(StartupWith(false, false) == "screen:legal");
    CHECK(StartupWith(true, true) == "screen:legal");
}

}
}
