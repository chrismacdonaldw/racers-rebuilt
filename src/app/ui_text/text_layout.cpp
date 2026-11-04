#include "app/ui_text/text_layout.hpp"

#include <algorithm>
#include <optional>

#include "app/ui_text/drawn_glyphs.hpp"

namespace rr::app {
namespace {

constexpr char16_t kSpace = u' ';
constexpr char16_t kLineFeed = 0x0A;
constexpr char16_t kCarriageReturn = 0x0D;
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

bool IsLineBreak(char16_t character) {
    return character == kLineFeed || character == kCarriageReturn;
}

struct Mark {
    const assets::Glyph* glyph = nullptr;
    std::optional<DrawnGlyph> drawn;
    float widthPx = 0.0F;
};

Mark GlyphMark(const assets::Glyph& glyph) {
    return {.glyph = &glyph, .widthPx = static_cast<float>(glyph.widthPx)};
}

std::optional<Mark> MarkFor(const UiFont& font, char16_t character) {
    if (const assets::Glyph* glyph = GlyphFor(font.glyphs, character)) {
        return GlyphMark(*glyph);
    }
    if (IsLineBreak(character)) {
        return std::nullopt;
    }
    if (font.missingGlyphs == MissingGlyphs::FirstGlyph) {
        return font.glyphs.glyphs.empty()
                   ? std::nullopt
                   : std::optional<Mark>(GlyphMark(font.glyphs.glyphs.front()));
    }
    if (const std::optional<DrawnGlyph> drawn = DrawnGlyphFor(character)) {
        return Mark{.drawn = drawn,
                    .widthPx = DrawnGlyphWidth(*drawn, static_cast<float>(font.heightPx))};
    }
    return std::nullopt;
}

template <typename Visit>
float ForEachGlyph(const UiFont& font, std::u16string_view text, Visit&& visit) {
    const auto spacing = static_cast<float>(font.spacingPx);
    float pen = 0.0F;
    bool hasAdvanced = false;
    for (const char16_t character : text) {
        const std::optional<Mark> mark =
            character == kSpace ? std::nullopt : MarkFor(font, character);
        if (character != kSpace && !mark) {
            continue;
        }
        if (hasAdvanced) {
            pen += spacing;
        }
        hasAdvanced = true;
        if (!mark) {
            pen += static_cast<float>(font.glyphs.spaceWidthPx);
            continue;
        }
        visit(*mark, pen);
        pen += mark->widthPx;
    }
    return pen;
}

float Advance(const UiFont& font, char16_t character) {
    if (character == kSpace) {
        return static_cast<float>(font.glyphs.spaceWidthPx) + static_cast<float>(font.spacingPx);
    }
    const std::optional<Mark> mark = MarkFor(font, character);
    return mark ? mark->widthPx + static_cast<float>(font.spacingPx) : 0.0F;
}

class LineBreaker {
public:
    LineBreaker(const UiFont& font, std::u16string_view text, float width)
        : font_(&font), text_(text), width_(width) {}

    [[nodiscard]] bool Done() const { return at_ >= text_.size(); }

    std::u16string_view Next() {
        if (!afterBreak_) {
            while (At(at_) == kSpace) {
                ++at_;
            }
        }
        const std::size_t start = at_;
        Fill();
        const std::u16string_view line = text_.substr(start, at_ - start);
        afterBreak_ = false;
        for (const char16_t lineBreak : {kCarriageReturn, kLineFeed}) {
            if (At(at_) == lineBreak) {
                ++at_;
                afterBreak_ = true;
            }
        }
        return line;
    }

private:
    [[nodiscard]] char16_t At(std::size_t index) const {
        return index < text_.size() ? text_[index] : char16_t{};
    }

    void Fill() {
        const std::size_t start = at_;
        std::size_t breakAt = at_;
        float word = 0.0F;
        float line = 0.0F;
        while ((line <= width_ || at_ == start) && !Done() && !IsLineBreak(text_[at_])) {
            if (text_[at_] == kSpace && word != 0.0F && At(at_ - 1) != kSpace) {
                breakAt = at_;
                word = 0.0F;
            }
            const float advance = Advance(*font_, text_[at_++]);
            word += advance;
            line += advance;
        }
        if (line <= width_) {
            return;
        }
        if (word != line) {
            at_ = breakAt;
            return;
        }
        while (line > width_ && at_ > breakAt + 1) {
            line -= Advance(*font_, text_[--at_]);
        }
    }

    const UiFont* font_;
    std::u16string_view text_;
    float width_;
    std::size_t at_ = 0;
    bool afterBreak_ = true;
};

}

float TextWidth(const UiFont& font, std::u16string_view text) {
    return ForEachGlyph(font, text, [](const Mark&, float) {});
}

void LayoutText(const UiFont& font, std::u16string_view text, const TextPlacement& placement,
                std::vector<render::SpriteDraw>& out) {
    const auto imageWidth = static_cast<float>(font.widthPx);
    ForEachGlyph(font, text, [&](const Mark& mark, float pen) {
        const render::Rect cell{.x = placement.x + (pen * placement.scale),
                                .y = placement.y,
                                .width = mark.widthPx * placement.scale,
                                .height = static_cast<float>(font.heightPx) * placement.scale};
        if (mark.glyph == nullptr) {
            DrawGlyphShape(mark.drawn.value_or(DrawnGlyph::Dash), cell, placement.tint, out);
            return;
        }
        out.push_back({.texture = font.texture,
                       .bounds = cell,
                       .uv = {.x = static_cast<float>(mark.glyph->leftPx) / imageWidth,
                              .width = mark.widthPx / imageWidth,
                              .height = 1.0F},
                       .tint = placement.tint});
    });
}

std::vector<std::u16string_view> WrapText(const UiFont& font, std::u16string_view text,
                                          float width) {
    std::vector<std::u16string_view> lines;
    LineBreaker breaker(font, text, width);
    while (!breaker.Done()) {
        lines.push_back(breaker.Next());
    }
    return lines;
}

}
