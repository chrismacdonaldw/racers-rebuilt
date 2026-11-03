#include "app/menu/build_help.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "app/ui_text/text_layout.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kRestToShow{3000};
constexpr Milliseconds kShownFor{20000};
constexpr Milliseconds kQuickShow{250};
constexpr Milliseconds kSuppressed{-1};
constexpr Milliseconds kStarted{1};
constexpr float kBorder = 4.0F;
constexpr float kNarrowest = 1.0F / 8.0F;
constexpr render::Color kBorderColor{.r = 0x20 / 255.0F, .g = 0x1C / 255.0F, .b = 0xE0 / 255.0F};
constexpr render::Color kPanelColor{.r = 0.0F, .g = 0.0F, .b = 0x38 / 255.0F};

struct Span {
    float near = 0.0F;
    float far = 0.0F;
};

struct Sides {
    Span across;
    Span down;
};

Sides SidesOf(const render::Rect& rect) {
    return {.across = {.near = rect.x, .far = rect.x + rect.width},
            .down = {.near = rect.y, .far = rect.y + rect.height}};
}

float Centred(Span target, Span screen, float size, float gap) {
    const float centred = std::floor((target.near + target.far - size) / 2.0F);
    if (centred < screen.near + gap) {
        return screen.near + gap;
    }
    return std::min(centred, screen.far - size - gap);
}

std::optional<float> Beside(Span target, Span screen, float size, float gap) {
    if (size + (2.0F * gap) < screen.far - target.far) {
        return target.far + gap;
    }
    return std::nullopt;
}

std::optional<float> Before(Span target, Span screen, float size, float gap) {
    if (size + (2.0F * gap) < target.near - screen.near) {
        return target.near - size - gap;
    }
    return std::nullopt;
}

render::Rect Place(const render::Rect& target, const render::Rect& screen, float width,
                   float height, float gap) {
    const Sides t = SidesOf(target);
    const Sides s = SidesOf(screen);
    if (const std::optional<float> x = Beside(t.across, s.across, width, gap)) {
        return {
            .x = *x, .y = Centred(t.down, s.down, height, gap), .width = width, .height = height};
    }
    if (const std::optional<float> y = Beside(t.down, s.down, height, gap)) {
        return {.x = Centred(t.across, s.across, width, gap),
                .y = *y,
                .width = width,
                .height = height};
    }
    if (const std::optional<float> y = Before(t.down, s.down, height, gap)) {
        return {.x = Centred(t.across, s.across, width, gap),
                .y = *y,
                .width = width,
                .height = height};
    }
    if (const std::optional<float> x = Before(t.across, s.across, width, gap)) {
        return {
            .x = *x, .y = Centred(t.down, s.down, height, gap), .width = width, .height = height};
    }
    return {.x = screen.x + std::floor((screen.width - width) / 2.0F),
            .y = screen.y + std::floor((screen.height - height) / 2.0F),
            .width = width,
            .height = height};
}

render::Rect Grown(const render::Rect& rect, float by) {
    return {.x = rect.x - by,
            .y = rect.y - by,
            .width = rect.width + (2.0F * by),
            .height = rect.height + (2.0F * by)};
}

}

void HelpTimer::Tick(Milliseconds elapsed, std::size_t target, bool pressed, bool hasHelp) {
    if (target != target_) {
        quick_ = IsShown() ? kStarted : Milliseconds{0};
        shown_ = Milliseconds{0};
        resting_ = Milliseconds{0};
        target_ = target;
    } else if (pressed && shown_ == Milliseconds{0}) {
        resting_ = Milliseconds{0};
        quick_ = Milliseconds{0};
    }
    if (quick_ > Milliseconds{0}) {
        quick_ += elapsed;
        if (quick_ > kQuickShow) {
            quick_ = Milliseconds{0};
        }
    }
    if (shown_ == Milliseconds{0}) {
        resting_ += elapsed;
        if (resting_ >= kRestToShow || quick_ > Milliseconds{0}) {
            resting_ = kRestToShow;
            quick_ = Milliseconds{0};
            shown_ = hasHelp ? kStarted : kSuppressed;
        }
    } else if (IsShown()) {
        shown_ += elapsed;
        if (shown_ > kShownFor) {
            shown_ = kSuppressed;
        }
    }
}

HelpBox LayoutHelp(const UiFont& font, std::u16string_view text, const render::Rect& target,
                   const render::Rect& screen) {
    const auto lineHeight = static_cast<float>(font.heightPx);
    const float widest = screen.width - (2.0F * lineHeight);
    const float wrap =
        std::clamp(std::floor(std::sqrt((lineHeight + 1.0F) * 3.0F * TextWidth(font, text))),
                   std::floor(widest * kNarrowest), widest);
    HelpBox box{.lines = WrapText(font, text, wrap)};
    float width = 0.0F;
    for (const std::u16string_view line : box.lines) {
        width = std::max(width, TextWidth(font, line));
    }
    const float height = static_cast<float>(box.lines.size()) * lineHeight;
    const render::Rect placed = Place(target, screen, width, height, lineHeight);
    box.x = placed.x;
    box.y = placed.y;
    box.panel = Grown(placed, std::floor(lineHeight / 2.0F));
    box.border = Grown(box.panel, kBorder);
    return box;
}

void DrawHelp(const UiFont& font, const HelpBox& box, std::vector<render::SpriteDraw>& out) {
    out.push_back({.bounds = box.border, .tint = kBorderColor});
    out.push_back({.bounds = box.panel, .tint = kPanelColor});
    const auto lineHeight = static_cast<float>(font.heightPx);
    float y = box.y;
    for (const std::u16string_view line : box.lines) {
        LayoutText(font, line, {.x = box.x, .y = y}, out);
        y += lineHeight;
    }
}

}
