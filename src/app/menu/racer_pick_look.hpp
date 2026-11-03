#pragma once

#include <array>
#include <string_view>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_widgets.hpp"
#include "assets/menu_layout.hpp"
#include "assets/menu_styles.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

struct RacerPickLook {
    MenuPicture backdrop;
    float bannerTop = 0.0F;
    MenuTextStyle banner;
    MenuTextStyle label;
    std::array<MenuButtonState, assets::kControlStates> configButton;
    assets::SoundIds configSounds{};
    MenuPoint config;
    ImageButtonLook startButton;
    MenuPoint start;
    ImageButtonLook backButton;
    MenuPoint back;
    SelectorArrows arrows;
    SelectorPlace picker;
    assets::SoundIds pickerSounds{};
    assets::CarouselSounds scrollSounds{};
};

inline constexpr std::string_view kRacerPickScreen = "PICKRCR";

[[nodiscard]] Result<RacerPickLook> LoadRacerPickLook(render::Renderer& renderer,
                                                      const assets::Vfs& vfs,
                                                      const MenuFiles& files);

}
