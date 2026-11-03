#pragma once

#include <array>
#include <string_view>

#include "app/menu/carousel_layout.hpp"
#include "app/menu/carousel_view.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/menu/part_carousel.hpp"
#include "app/menu/racer_edit_flow.hpp"
#include "assets/menu_layout.hpp"
#include "assets/menu_styles.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

inline constexpr std::string_view kEditDriverScreen = "EDITDRVR";

struct PartRow {
    SelectorPlace selector;
    CarouselLayout carousel;
};

struct EditDriverLook {
    MenuPicture backdrop;
    MenuTextStyle banner;
    float bannerTop = 0.0F;
    SelectorArrows arrows;
    MenuFrameLook box;
    std::array<PartRow, kDriverSlots> rows;
    CarouselColors itemColors{};
    assets::SoundIds rowSounds{};
    assets::CarouselSounds scrollSounds{};
    Milliseconds scroll{0};
    std::array<MenuButtonState, assets::kControlStates> mixButton;
    assets::SoundIds mixSounds{};
    MenuPoint mix;
    ImageButtonLook done;
    MenuPoint donePlace;
    ImageButtonLook back;
    MenuPoint backPlace;
};

[[nodiscard]] Result<EditDriverLook> LoadEditDriverLook(render::Renderer& renderer,
                                                        const assets::Vfs& vfs,
                                                        const MenuFiles& files,
                                                        const EditorButtons& buttons);

}
