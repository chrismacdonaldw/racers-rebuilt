#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "app/menu/dialog_layout.hpp"
#include "app/menu/menu_files.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_parts.hpp"
#include "app/menu/menu_text.hpp"
#include "app/ui_text/ui_image.hpp"
#include "assets/menu_layout.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"

namespace rr::app {

struct MenuTextStyle {
    std::string font;
    render::Color color;
};

struct ImageButtonLook {
    std::array<MenuButtonState, assets::kControlStates> states;
    BoxSize image;
};

struct ArrowImages {
    UiImage normal;
    UiImage focused;
};

struct SelectorArrows {
    ArrowImages previous;
    ArrowImages next;
};

/// The arrows sit at their own image sizes.
struct SelectorPlace {
    render::Rect bounds;
    render::Rect previous;
    render::Rect next;
};

[[nodiscard]] render::Rect SizedAt(MenuPoint origin, const UiImage& image);
[[nodiscard]] render::Rect SpanOf(MenuPoint origin, const assets::MenuRect& rect);

/// `screen` names the file in the error.
[[nodiscard]] Result<const assets::MenuElement*> FindMenuElement(const MenuFiles& files,
                                                                 std::string_view screen,
                                                                 std::string_view name);

[[nodiscard]] Result<MenuTextStyle> ReadTextStyle(const MenuFiles& files, std::string_view name);

/// Sized by the normal state's image.
[[nodiscard]] Result<ImageButtonLook> LoadImageButton(MenuImageCache& images,
                                                      const MenuFiles& files,
                                                      std::string_view style);

[[nodiscard]] Result<SelectorArrows> LoadSelectorArrows(MenuImageCache& images);

[[nodiscard]] Result<SelectorPlace> PlaceSelector(const MenuFiles& files, std::string_view screen,
                                                  std::string_view name,
                                                  const SelectorArrows& arrows);

/// The label sits right of the image, centred on it.
void DrawImageButton(const MenuText& text, const ImageButtonLook& look, std::size_t state,
                     std::u16string_view label, MenuPoint at, std::vector<render::SpriteDraw>& out);

void DrawSelectorArrows(const SelectorArrows& arrows, const SelectorPlace& place, bool isFocused,
                        std::vector<render::SpriteDraw>& out);

/// Right-aligned 20 units in from the backdrop's right edge, as the original's banners are.
void DrawBanner(const MenuText& text, const MenuTextStyle& style, std::u16string_view title,
                const MenuPicture& backdrop, float top, std::vector<render::SpriteDraw>& out);

}
