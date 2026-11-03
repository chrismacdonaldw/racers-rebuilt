#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "assets/image_list.hpp"
#include "assets/menu_layout.hpp"
#include "assets/menu_styles.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

inline constexpr std::string_view kMenuFolder = "MENUDATA";
inline constexpr std::string_view kMenuFontList = "GFONTS.FDB";

/// Shared styles and images come first, then the screen's own.
struct MenuFiles {
    assets::MenuLayout layout;
    std::vector<assets::MenuStyles> styles;
    std::vector<assets::ImageListEntry> images;

    [[nodiscard]] const assets::ImageStyle* FindImageStyle(std::string_view name) const;
    [[nodiscard]] const assets::TextStyle* FindTextStyle(std::string_view name) const;
    [[nodiscard]] const assets::CarouselStyle* FindCarouselStyle(std::string_view name) const;
    [[nodiscard]] const assets::SelectorStyle* FindSelectorStyle(std::string_view name) const;
    [[nodiscard]] const assets::GameButtonStyle* FindGameButtonStyle(std::string_view name) const;
    [[nodiscard]] const assets::FrameStyle* FindFrameStyle(std::string_view name) const;
    [[nodiscard]] const assets::ImageListEntry* FindImage(std::string_view name) const;
};

/// `screen` is a base name from `MENUNAME.SRF`, such as `MAINMENU`.
[[nodiscard]] Result<MenuFiles> ReadMenuFiles(const assets::Vfs& vfs, std::string_view screen);

[[nodiscard]] Result<std::vector<std::u16string>> ReadMenuText(const assets::Vfs& vfs,
                                                               std::string_view language);

[[nodiscard]] std::string MenuLanguageFolder(std::string_view language);

}
