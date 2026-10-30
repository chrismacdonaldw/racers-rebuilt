#pragma once

#include <array>
#include <cstddef>
#include <string>

#include "assets/menu_layout.hpp"
#include "render/render_view.hpp"

namespace rr::app {

struct MenuPicture {
    render::TextureId texture{};
    render::Rect bounds;
    render::Color tint;
};

struct MenuButtonState {
    render::TextureId image{};
    std::string font;
    render::Color textColor;
};

struct MenuButton {
    std::string name;
    std::size_t textLine = 0;
    render::Rect bounds;
    std::array<MenuButtonState, assets::kControlStates> states;
};

}
