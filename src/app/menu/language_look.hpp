#pragma once

#include "app/menu/menu_page.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_widgets.hpp"
#include "assets/menu_layout.hpp"
#include "assets/menu_styles.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

struct LanguageLook {
    MenuPicture backdrop;
    float bannerTop = 0.0F;
    MenuTextStyle banner;
    MenuTextStyle label;
    ImageButtonLook backButton;
    MenuPoint back;
    SelectorArrows arrows;
    SelectorPlace picker;
    assets::SoundIds pickerSounds{};
    assets::CarouselSounds scrollSounds{};
};

[[nodiscard]] Result<LanguageLook> LoadLanguageLook(render::Renderer& renderer,
                                                    const assets::Vfs& vfs);

}
