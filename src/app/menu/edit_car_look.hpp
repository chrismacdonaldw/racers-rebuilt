#pragma once

#include <array>
#include <string_view>

#include "app/menu/car_edit.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/menu/racer_edit_flow.hpp"
#include "assets/menu_styles.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

inline constexpr std::string_view kEditCarScreen = "EDITCAR";
inline constexpr std::size_t kCarTextButtons = 3;
inline constexpr std::array<std::string_view, kPartSetCount> kSetImageStyles{
    "bricks",  "race",     "space",  "castle", "pirate", "islander",
    "magical", "adventur", "jungle", "alien",  "rr",     "vv"};

struct EditCarLook {
    MenuPicture backdrop;
    MenuTextStyle banner;
    float bannerTop = 0.0F;
    SelectorArrows arrows;
    SelectorPlace picker;
    MenuFrameLook box;
    render::Rect setBox;
    std::array<UiImage, kPartSetCount> sets;
    assets::SoundIds pickerSounds{};
    assets::CarouselSounds scrollSounds{};
    Milliseconds setSlide{0};
    std::array<MenuButtonState, assets::kControlStates> textButton;
    std::array<MenuPoint, kCarTextButtons> textPlaces;
    std::array<assets::SoundIds, kCarTextButtons> textSounds{};
    ImageButtonLook done;
    MenuPoint donePlace;
    ImageButtonLook back;
    MenuPoint backPlace;
};

/// The set icons live in `BUILDER.IDB`, which the screen's own files don't list.
[[nodiscard]] Result<void> AddBuilderImages(const assets::Vfs& vfs, MenuFiles& files);

[[nodiscard]] Result<EditCarLook> LoadEditCarLook(render::Renderer& renderer,
                                                  const assets::Vfs& vfs, const MenuFiles& files,
                                                  const EditorButtons& buttons);

}
