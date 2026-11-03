#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "app/menu/menu_widgets.hpp"
#include "render/render_view.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

/// An area that focuses `control` under the pointer and presses `click` when clicked.
struct PointerTarget {
    render::Rect area;
    std::size_t control = 0;
    std::optional<sim::MenuAction> click = sim::MenuAction::Confirm;
};

struct PointerChoice {
    std::optional<std::size_t> hovered;
    std::optional<std::size_t> clicked;
    std::optional<sim::MenuAction> click;

    [[nodiscard]] std::optional<std::size_t> Focus() const { return clicked ? clicked : hovered; }
    [[nodiscard]] bool Clicks(sim::MenuAction action) const { return click == action; }
};

[[nodiscard]] std::optional<std::size_t> HitTarget(std::span<const PointerTarget> targets,
                                                   const sim::MenuPointer& pointer);

class PointerButtons {
public:
    /// `hovered` is set only on ticks the pointer moved.
    PointerChoice Tick(const sim::MenuPointer& pointer, std::span<const PointerTarget> targets);

private:
    std::optional<std::size_t> down_;
};

/// The arrows step `control` left and right, and the rest of the selector focuses it.
void AddSelectorTargets(const SelectorPlace& place, std::size_t control,
                        std::vector<PointerTarget>& out);

[[nodiscard]] sim::InputFrame WithClick(const sim::InputFrame& input, const PointerChoice& choice);

}
