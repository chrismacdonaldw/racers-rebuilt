#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_frame.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/menu/racer_edit_flow.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/menu_layout.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"
#include "settings/progress.hpp"

namespace rr::app {

inline constexpr std::string_view kLicenceScreen = "DRVRLICE";
inline constexpr std::size_t kLicenceTrophies = 8;

using NameSounds = std::array<std::int32_t, 9>;

struct NameBox {
    render::Rect bounds;
    std::string font;
    std::size_t maxLength = 0;
    NameSounds sounds{};
    assets::SoundIds focusSounds{};
    MenuFrameLook frame;
    render::Rect frameBounds;
};

struct LicenceLook {
    MenuPicture backdrop;
    MenuPicture licence;
    MenuTextStyle banner;
    float bannerTop = 0.0F;
    NameBox name;
    std::array<UiImage, 3> trophies;
    std::array<MenuPoint, kLicenceTrophies> trophySpots;
    std::array<MenuButtonState, assets::kControlStates> expressionButton;
    assets::SoundIds expressionSounds{};
    MenuPoint expression;
    ImageButtonLook done;
    MenuPoint donePlace;
    ImageButtonLook back;
    MenuPoint backPlace;
};

[[nodiscard]] Result<LicenceLook> LoadLicenceLook(render::Renderer& renderer,
                                                  const assets::Vfs& vfs, const MenuFiles& files,
                                                  const EditorButtons& buttons);

[[nodiscard]] MenuPicture TrophyPicture(const LicenceLook& look, std::size_t circuit,
                                        settings::Trophy trophy);

}
