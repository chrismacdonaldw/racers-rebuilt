#pragma once

#include <optional>
#include <string>
#include <vector>

#include "app/racer/champion_car.hpp"
#include "app/scene.hpp"
#include "render/render_view.hpp"
#include "settings/racers.hpp"

namespace rr::app {

/// `folder` is under `MENUDATA`, `rect` in UI units. `playerRacer`, else `driver`, replaces `guy1`.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct CutscenePlay {
    std::string folder;
    std::string file;
    std::optional<render::Rect> rect;
    bool isLooping = true;
    bool isSkippable = false;
    SceneMusic music;
    std::string next;
    std::string driver;
    std::optional<settings::Racer> playerRacer;
    bool showsChampionCar = false;
    CarBody carBody = CarBody::Bricks;
    std::vector<std::string> hiddenTexts;
};

/// Who replaces `guy1` and `carbody` when a screen shows the player's racer.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct AwardCast {
    std::optional<settings::Racer> playerRacer;
    CarBody carBody = CarBody::Bricks;
};

}
