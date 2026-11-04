#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "app/ui_text/text_fixes.hpp"
#include "assets/font_list.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"

namespace rr::app {

struct UiFont {
    render::TextureId texture{};
    std::uint32_t widthPx = 0;
    std::uint32_t heightPx = 0;
    std::int32_t spacingPx = 0;
    assets::FontGlyphs glyphs;
    std::optional<assets::FontGlyphs> fixedGlyphs;
    TextFixes fixes;

    [[nodiscard]] const assets::FontGlyphs& Glyphs() const {
        return fixes.fixesGlyphLists && fixedGlyphs ? *fixedGlyphs : glyphs;
    }
};

/// Keyed by font name in upper case.
using UiFonts = std::map<std::string, UiFont, std::less<>>;

/// Reads the font's image from `folder`, the folder of its font list.
[[nodiscard]] Result<UiFont> LoadUiFont(render::Renderer& renderer, const assets::Vfs& vfs,
                                        std::string_view folder,
                                        const assets::FontDefinition& definition);

[[nodiscard]] Result<UiFonts> LoadUiFonts(render::Renderer& renderer, const assets::Vfs& vfs,
                                          std::string_view folder, std::string_view fontList);

[[nodiscard]] const UiFont* FindUiFont(const UiFonts& fonts, std::string_view name);

void UseTextFixes(UiFonts& fonts, TextFixes fixes);

}
