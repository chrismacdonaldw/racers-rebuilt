#pragma once

#include <optional>
#include <string>

#include "app/scene.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// `folder` is under `MENUDATA`. `rect` is in UI units, unset to fill the window.
struct CutscenePlay {
    std::string folder;
    std::string file;
    std::optional<render::Rect> rect;
    bool isLooping = true;
    bool isSkippable = false;
    SceneMusic music;
    std::string next;
};

}
