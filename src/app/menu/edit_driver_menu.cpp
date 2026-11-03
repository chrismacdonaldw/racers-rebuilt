#include "app/menu/edit_driver_menu.hpp"

namespace rr::app {
namespace {

constexpr int kRows = static_cast<int>(DriverRow::Back) + 1;

}

DriverRequest EditDriverMenu::Tick(const sim::InputFrame& input) {
    if (input.WasPressed(sim::MenuAction::Back)) {
        return DriverRequest::Leave;
    }
    if (input.WasPressed(sim::MenuAction::Up)) {
        MoveFocus(-1);
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        MoveFocus(1);
    }
    if (Slot()) {
        const int direction = (input.WasPressed(sim::MenuAction::Right) ? 1 : 0) -
                              (input.WasPressed(sim::MenuAction::Left) ? 1 : 0);
        if (direction != 0) {
            direction_ = direction;
            return DriverRequest::Step;
        }
    }
    if (!input.WasPressed(sim::MenuAction::Confirm)) {
        return DriverRequest::None;
    }
    switch (focus_) {
        case DriverRow::Mix:
            return DriverRequest::Mix;
        case DriverRow::Done:
            return DriverRequest::Done;
        case DriverRow::Back:
            return DriverRequest::Leave;
        default:
            return DriverRequest::None;
    }
}

std::optional<DriverSlot> EditDriverMenu::Slot() const {
    if (focus_ > DriverRow::Legs) {
        return std::nullopt;
    }
    return static_cast<DriverSlot>(focus_);
}

void EditDriverMenu::MoveFocus(int step) {
    focus_ = static_cast<DriverRow>((static_cast<int>(focus_) + step + kRows) % kRows);
}

}
