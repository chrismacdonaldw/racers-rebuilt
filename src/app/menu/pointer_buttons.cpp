#include "app/menu/pointer_buttons.hpp"

#include <algorithm>
#include <utility>

#include "app/ui_text/text_layout.hpp"

namespace rr::app {

std::optional<std::size_t> HitTarget(std::span<const PointerTarget> targets,
                                     const sim::MenuPointer& pointer) {
    if (!pointer.isOver) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const render::Rect& area = targets[i].area;
        if (pointer.x >= area.x && pointer.y >= area.y && pointer.x < area.x + area.width &&
            pointer.y < area.y + area.height) {
            return i;
        }
    }
    return std::nullopt;
}

PointerChoice PointerButtons::Tick(const sim::MenuPointer& pointer,
                                   std::span<const PointerTarget> targets) {
    const std::optional<std::size_t> hit = HitTarget(targets, pointer);
    PointerChoice choice;
    if (pointer.moved && hit) {
        choice.hovered = targets[*hit].control;
    }
    if (pointer.pressed) {
        down_ = hit;
    }
    if (pointer.released) {
        const std::optional<std::size_t> down = std::exchange(down_, std::nullopt);
        if (down && down == hit) {
            choice.clicked = targets[*hit].control;
            choice.click = targets[*hit].click;
        }
    }
    return choice;
}

void AddSelectorTargets(const SelectorPlace& place, std::size_t control,
                        std::vector<PointerTarget>& out) {
    out.push_back({.area = place.previous, .control = control, .click = sim::MenuAction::Left});
    out.push_back({.area = place.next, .control = control, .click = sim::MenuAction::Right});
    out.push_back({.area = place.bounds, .control = control, .click = std::nullopt});
}

render::Rect Spanning(const render::Rect& a, const render::Rect& b) {
    const float left = std::min(a.x, b.x);
    const float top = std::min(a.y, b.y);
    return {.x = left,
            .y = top,
            .width = std::max(a.x + a.width, b.x + b.width) - left,
            .height = std::max(a.y + a.height, b.y + b.height) - top};
}

render::Rect LabelArea(const MenuText& text, const MenuButtonState& state,
                       std::u16string_view label, MenuPoint at) {
    const UiFont* font = text.Font(state.font);
    if (font == nullptr) {
        return {.x = at.x, .y = at.y};
    }
    return {.x = at.x,
            .y = at.y,
            .width = TextWidth(*font, label),
            .height = static_cast<float>(font->heightPx)};
}

render::Rect ImageButtonArea(const MenuText& text, const ImageButtonLook& look, std::size_t state,
                             std::u16string_view label, MenuPoint at) {
    const render::Rect image{
        .x = at.x, .y = at.y, .width = look.image.width, .height = look.image.height};
    const render::Rect shown =
        LabelArea(text, look.states.at(state), label, {.x = at.x + look.image.width, .y = at.y});
    return Spanning(image, {.x = shown.x, .y = at.y, .width = shown.width, .height = image.height});
}

sim::InputFrame WithClick(const sim::InputFrame& input, const PointerChoice& choice) {
    sim::InputFrame clicked = input;
    if (choice.click) {
        clicked.menuPressed.set(sim::InputFrame::Index(*choice.click));
    }
    return clicked;
}

}
