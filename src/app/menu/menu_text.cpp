#include "app/menu/menu_text.hpp"

#include <utility>

#include "app/fix_settings.hpp"
#include "app/game_settings.hpp"
#include "app/menu/menu_files.hpp"
#include "core/log.hpp"
#include "core/result.hpp"

namespace rr::app {

bool MenuText::Apply(render::Renderer& renderer, const assets::Vfs& vfs,
                     const settings::Registry& settings) {
    std::string language = SelectedLanguage(settings);
    const bool isNewLanguage = language != language_;
    if (isNewLanguage) {
        Load(renderer, vfs, std::move(language));
    }
    UseMissingGlyphs(fonts_, MissingGlyphsFrom(settings));
    return isNewLanguage;
}

void MenuText::Load(render::Renderer& renderer, const assets::Vfs& vfs, std::string language) {
    language_ = std::move(language);
    Result<UiFonts> fonts =
        LoadUiFonts(renderer, vfs, MenuLanguageFolder(language_), kMenuFontList);
    Result<std::vector<std::u16string>> lines = ReadMenuText(vfs, language_);
    if (!fonts || !lines) {
        log::Error("menus have no text: {}", (fonts ? lines.error() : fonts.error()).message);
    }
    fonts_ = std::move(fonts).value_or(UiFonts{});
    lines_ = std::move(lines).value_or(std::vector<std::u16string>{});
}

std::u16string_view MenuText::Line(std::size_t line) const {
    return line < lines_.size() ? std::u16string_view(lines_[line]) : std::u16string_view{};
}

}
