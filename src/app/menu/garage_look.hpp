#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "app/menu/garage_menu.hpp"
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

inline constexpr std::string_view kGarageScreen = "GARAGE";
inline constexpr std::size_t kGarageButtons = static_cast<std::size_t>(GarageButton::Back);

struct GarageLook {
    MenuPicture backdrop;
    float bannerTop = 0.0F;
    MenuTextStyle banner;
    MenuTextStyle label;
    std::array<MenuButtonState, assets::kControlStates> button;
    assets::SoundIds buttonSounds{};
    std::array<MenuPoint, kGarageButtons> buttons;
    ImageButtonLook backButton;
    MenuPoint back;
    SelectorArrows arrows;
    SelectorPlace picker;
    assets::CarouselSounds scrollSounds{};
};

[[nodiscard]] Result<GarageLook> LoadGarageLook(render::Renderer& renderer, const assets::Vfs& vfs,
                                                const MenuFiles& files);

}
