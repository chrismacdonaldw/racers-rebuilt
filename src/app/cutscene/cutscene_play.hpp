#pragma once

#include <optional>
#include <string>
#include <vector>

#include "app/scene.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// `folder` is under `MENUDATA`, `rect` in UI units or unset to fill. `driver` replaces `guy1`.
struct CutscenePlay {
    std::string folder;
    std::string file;
    std::optional<render::Rect> rect;
    bool isLooping = true;
    bool isSkippable = false;
    SceneMusic music;
    std::string next;
    std::string driver;
    std::vector<std::string> hiddenTexts;
};

}
