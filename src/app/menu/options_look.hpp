#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "app/menu/dialog_layout.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/menu/options_menu.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/menu_layout.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

struct OptionsButton {
    OptionsRow row = OptionsRow::Game;
    std::size_t textLine = 0;
    render::Rect bounds;
};

struct OptionsLabel {
    OptionsPage page = OptionsPage::Main;
    std::size_t textLine = 0;
    float x = 0.0F;
    float y = 0.0F;
};

struct OptionsSelector {
    OptionsRow row = OptionsRow::Opponents;
    SelectorPlace place;
};

struct OptionsSlider {
    OptionsRow row = OptionsRow::MusicVolume;
    render::Rect bounds;
};

struct OptionsLook {
    MenuPicture backdrop;
    float bannerTop = 0.0F;
    MenuTextStyle banner;
    MenuTextStyle label;
    MenuTextStyle driver;
    std::array<MenuButtonState, assets::kControlStates> textButton;
    ImageButtonLook backButton;
    std::vector<OptionsButton> buttons;
    std::vector<OptionsLabel> labels;
    std::vector<OptionsSelector> selectors;
    std::vector<OptionsSlider> sliders;
    SelectorArrows arrows;
    UiImage stepDown;
    UiImage stepUp;
    UiImage rail;
    UiImage thumb;
};

[[nodiscard]] Result<OptionsLook> LoadOptionsLook(render::Renderer& renderer,
                                                  const assets::Vfs& vfs);

struct SliderParts {
    render::Rect stepDown;
    render::Rect stepUp;
    render::Rect rail;
    render::Rect thumb;
};

/// The original's `OptionsRow` layout for `step` of 0 to `kMaxVolumeStep`.
[[nodiscard]] SliderParts LayOutSlider(const render::Rect& bounds, const OptionsLook& look,
                                       int step);

}
