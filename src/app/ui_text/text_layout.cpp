#include "app/ui_text/text_layout.hpp"

#include <algorithm>

namespace rr::app {
namespace {

constexpr char16_t kSpace = u' ';
constexpr char16_t kCaseOffset = 0x20;
constexpr char16_t kLatinLowerFirst = 0xE0;
constexpr char16_t kLatinLowerLast = 0xFE;
constexpr char16_t kDivisionSign = 0xF7;

bool IsLowerCase(char16_t character) {
    return (character >= u'a' && character <= u'z') ||
           (character >= kLatinLowerFirst && character <= kLatinLowerLast &&
            character != kDivisionSign);
}

const assets::Glyph* FindGlyph(const assets::FontGlyphs& glyphs, char16_t character) {
    const auto glyph =
        std::ranges::lower_bound(glyphs.glyphs, character, {}, &assets::Glyph::character);
    return glyph != glyphs.glyphs.end() && glyph->character == character ? &*glyph : nullptr;
}

const assets::Glyph* GlyphFor(const assets::FontGlyphs& glyphs, char16_t character) {
    const assets::Glyph* glyph = FindGlyph(glyphs, character);
    if (glyph == nullptr && IsLowerCase(character)) {
        return FindGlyph(glyphs, static_cast<char16_t>(character - kCaseOffset));
    }
    return glyph;
}

template <typename Visit>
float ForEachGlyph(const UiFont& font, std::u16string_view text, Visit&& visit) {
    const auto spacing = static_cast<float>(font.spacingPx);
    float pen = 0.0F;
    bool hasAdvanced = false;
    for (const char16_t character : text) {
        const assets::Glyph* glyph =
            character == kSpace ? nullptr : GlyphFor(font.glyphs, character);
        if (character != kSpace && glyph == nullptr) {
            continue;
        }
        if (hasAdvanced) {
            pen += spacing;
        }
        hasAdvanced = true;
        if (glyph == nullptr) {
            pen += static_cast<float>(font.glyphs.spaceWidthPx);
            continue;
        }
        visit(*glyph, pen);
        pen += static_cast<float>(glyph->widthPx);
    }
    return pen;
}

}

float TextWidth(const UiFont& font, std::u16string_view text) {
    return ForEachGlyph(font, text, [](const assets::Glyph&, float) {});
}

void LayoutText(const UiFont& font, std::u16string_view text, const TextPlacement& placement,
                std::vector<render::SpriteDraw>& out) {
    const auto imageWidth = static_cast<float>(font.widthPx);
    ForEachGlyph(font, text, [&](const assets::Glyph& glyph, float pen) {
        const auto width = static_cast<float>(glyph.widthPx);
        out.push_back({.texture = font.texture,
                       .bounds = {.x = placement.x + (pen * placement.scale),
                                  .y = placement.y,
                                  .width = width * placement.scale,
                                  .height = static_cast<float>(font.heightPx) * placement.scale},
                       .uv = {.x = static_cast<float>(glyph.leftPx) / imageWidth,
                              .width = width / imageWidth,
                              .height = 1.0F},
                       .tint = placement.tint});
    });
}

}
