#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "app/hud/hud_strings.hpp"
#include "app/hud/hud_text.hpp"
#include "app/ui_text/ui_font.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

inline constexpr std::string_view kHudFontList = "LEGOFNTS.FDB";

/// `GAMEDATA/COMMON/<language>`, which holds the race's fonts and text.
[[nodiscard]] std::string HudFolder(std::string_view language);

struct HudLanguage {
    HudStrings strings;
    std::vector<std::u16string> driverNames;
};

[[nodiscard]] Result<HudLanguage> LoadHudLanguage(const assets::Vfs& vfs,
                                                  std::string_view language);

/// Fails when `font_ths` or `ignum` is missing.
[[nodiscard]] Result<HudFonts> FindHudFonts(const UiFonts& fonts);

}
