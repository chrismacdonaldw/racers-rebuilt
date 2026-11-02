#include "app/cutscene/cutscene_screen.hpp"

#include <doctest/doctest.h>

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/menu_scenes.hpp"
#include "assets/archive_survey.hpp"
#include "settings/racers.hpp"

namespace rr::app {
namespace {

TEST_CASE("a cutscene screen name round trips with and without a next scene") {
    const CutsceneScreenSetup legal{.menu = "legal"};
    const CutsceneScreenSetup intro{.menu = "circuit3", .next = "circuit:c2:racer=1"};

    CHECK(CutsceneScreenName(legal) == "screen:legal");
    CHECK(CutsceneScreenName(intro) == "screen:circuit3>circuit:c2:racer=1");
    CHECK(ParseCutsceneScreen(CutsceneScreenName(legal)) == legal);
    CHECK(ParseCutsceneScreen(CutsceneScreenName(intro)) == intro);
    const CutsceneScreenSetup award{
        .menu = "c_award1", .driver = "JT", .next = "screen:wincar:driver=CR"};
    CHECK(CutsceneScreenName(award) == "screen:c_award1:driver=JT>screen:wincar:driver=CR");
    CHECK(ParseCutsceneScreen(CutsceneScreenName(award)) == award);
    CHECK(IsCutsceneScreen("screen:legal"));
    CHECK_FALSE(IsCutsceneScreen("cutscene:LEGAL/SPLASH1"));
}

TEST_CASE("a cutscene screen name fails on an unknown menu or with nowhere to go") {
    CHECK_FALSE(ParseCutsceneScreen("screen:splash").has_value());
    CHECK_FALSE(ParseCutsceneScreen("screen:circuit1").has_value());
}

TEST_CASE("each circuit's intro is the screen after its number") {
    CHECK(CircuitIntro("c0") == "circuit1");
    CHECK(CircuitIntro("c6") == "circuit7");
    CHECK_FALSE(CircuitIntro("c7").has_value());
    CHECK_FALSE(CircuitIntro("c").has_value());
    CHECK_FALSE(CircuitIntro("c1x").has_value());
    CHECK_FALSE(CircuitIntro("x").has_value());
}

TEST_CASE("a screen skips on a fresh press or any release, not a key repeat") {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    CHECK(IsSkipPress(event));
    event.key.repeat = true;
    CHECK_FALSE(IsSkipPress(event));
    event.type = SDL_EVENT_KEY_UP;
    CHECK(IsSkipPress(event));
    event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    CHECK(IsSkipPress(event));
    event.type = SDL_EVENT_MOUSE_MOTION;
    CHECK_FALSE(IsSkipPress(event));
}

std::string Placement(const assets::Vfs& vfs, std::string_view menu) {
    const std::optional<CutsceneScreen> screen = FindCutsceneScreen(menu);
    REQUIRE(screen.has_value());
    const Result<CutsceneScreenPlacement> placed = ReadCutsceneScreen(vfs, *screen);
    REQUIRE_MESSAGE(placed.has_value(), placed.error().message);
    return std::format("{}/{} at {} {} {}x{}", placed->folder, placed->cutscene, placed->rect.x,
                       placed->rect.y, placed->rect.width, placed->rect.height);
}

TEST_CASE("retail screens place their cutscenes as their MIB elements say") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    CHECK(Placement(*vfs, "legal") == "MENUDATA/LEGAL/SPLASH1 at 5 35 630x420");
    CHECK(Placement(*vfs, "credits") == "MENUDATA/CREDITS/CREDITS at 5 35 630x420");
    CHECK(Placement(*vfs, "circuit1") == "MENUDATA/CIRCUIT1/CIR1-01 at 0 0 640x480");
    CHECK(Placement(*vfs, "circuit7") == "MENUDATA/CIRCUIT7/FR at 0 0 640x480");
}

TEST_CASE("retail award and car win screens place their cutscenes full screen") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    std::vector<std::string> placements;
    for (const std::string_view menu :
         {"c_award1", "c_award2", "c_award3", "c_award4", "wincar", "winrrcar", "winvvcar"}) {
        placements.push_back(Placement(*vfs, menu));
    }
    CHECK(placements ==
          std::vector<std::string>{
              "MENUDATA/C_AWARD1/1STPLACE at 0 0 640x480", "MENUDATA/C_AWARD2/TWO at 0 0 640x480",
              "MENUDATA/C_AWARD3/3-01 at 0 0 640x480", "MENUDATA/C_AWARD4/LOSE at 0 0 640x480",
              "MENUDATA/WINCAR/WINCAR at 0 0 640x480", "MENUDATA/WINRRCAR/END at 0 0 640x480",
              "MENUDATA/WINVVCAR/VVWIN at 0 0 640x480"});
}

std::string Play(const assets::Vfs& vfs, std::string_view scene) {
    const Result<CutscenePlay> play = ScreenPlay(vfs, scene, std::nullopt);
    REQUIRE_MESSAGE(play.has_value(), play.error().message);
    return std::format("{} looping={} skippable={} music='{}' next={}", play->file, play->isLooping,
                       play->isSkippable, play->music.track, play->next);
}

TEST_CASE("a retail screen plays once and goes on to its next scene") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    CHECK(Play(*vfs, "screen:legal") ==
          std::format("SPLASH1 looping=false skippable=true music='' next={}", kMainMenuScene));
    CHECK(Play(*vfs, "screen:circuit1>circuit:c0") ==
          "CIR1-01 looping=false skippable=true music='circuit1' next=circuit:c0");
}

TEST_CASE("award screens can't be skipped, and the Rocket Racer win leads to the credits") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    CHECK(Play(*vfs, "screen:c_award1>screen:wincar") ==
          "1STPLACE looping=false skippable=false music='1st' next=screen:wincar");
    CHECK(Play(*vfs, "screen:winrrcar") ==
          "END looping=false skippable=false music='rrcar' next=screen:credits");
    CHECK(Play(*vfs, "screen:credits>menu:options") ==
          "CREDITS looping=false skippable=true music='credits' next=menu:options");
}

std::string Racer(const assets::Vfs& vfs, std::string_view scene) {
    const Result<CutscenePlay> play = ScreenPlay(vfs, scene, settings::Racer{.name = "MINE"});
    REQUIRE(play.has_value());
    std::string racer = std::format("driver='{}' player='{}' hides", play->driver,
                                    play->playerRacer ? play->playerRacer->name : "");
    for (const std::string& text : play->hiddenTexts) {
        racer += std::format(" {}", text);
    }
    return racer;
}

TEST_CASE("only racer screens take a driver, and wincar shows its champion, not the player") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    CHECK(Racer(*vfs, "screen:wincar:driver=KK") ==
          "driver='KK' player='' hides textCR textBB textJT textGM textBVB");
    CHECK(Racer(*vfs, "screen:winvvcar:driver=KK") == "driver='' player='' hides");
    CHECK(Racer(*vfs, "screen:c_award2:driver=KK") == "driver='KK' player='MINE' hides");
}

}
}
