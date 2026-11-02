#pragma once

#include <array>

#include "app/menu/control_bindings.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/menu_layout.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

struct ControlsLook {
    MenuPicture backdrop;
    float bannerTop = 0.0F;
    MenuTextStyle banner;
    MenuTextStyle label;
    std::array<MenuButtonState, assets::kControlStates> bindingButton;
    ImageButtonLook finishButton;
    MenuPoint finish;
    std::array<MenuPoint, kControlRows> bindings{};
    std::array<MenuPoint, kControlRows> labels{};
    SelectorArrows arrows;
    SelectorPlace devicePicker;
    MenuPoint deviceImage;
    UiImage keyboard;
    UiImage gamepad;
};

[[nodiscard]] Result<ControlsLook> LoadControlsLook(render::Renderer& renderer,
                                                    const assets::Vfs& vfs);

}
