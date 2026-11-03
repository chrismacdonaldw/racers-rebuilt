#include "app/menu/edit_car_menu.hpp"

namespace rr::app {
namespace {

std::size_t IndexOf(CarRow row) {
    return static_cast<std::size_t>(row);
}

}

CarRequest EditCarMenu::Tick(const sim::InputFrame& input) {
    if (input.WasPressed(sim::MenuAction::Back)) {
        return CarRequest::Leave;
    }
    if (input.WasPressed(sim::MenuAction::Up)) {
        MoveFocus(-1);
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        MoveFocus(1);
    }
    if (focus_ == CarRow::PartSet && IsEnabled(CarRow::PartSet)) {
        if (input.WasPressed(sim::MenuAction::Left)) {
            return CarRequest::PreviousSet;
        }
        if (input.WasPressed(sim::MenuAction::Right)) {
            return CarRequest::NextSet;
        }
    }
    return input.WasPressed(sim::MenuAction::Confirm) ? Choose() : CarRequest::None;
}

void EditCarMenu::Enable(CarRow row, bool isEnabled) {
    enabled_.at(IndexOf(row)) = isEnabled;
    if (!isEnabled && focus_ == row) {
        MoveFocus(1);
    }
}

bool EditCarMenu::IsEnabled(CarRow row) const {
    return enabled_.at(IndexOf(row));
}

bool EditCarMenu::FocusOn(CarRow row) {
    if (!IsEnabled(row)) {
        return false;
    }
    focus_ = row;
    return true;
}

void EditCarMenu::MoveFocus(int step) {
    const auto count = static_cast<int>(kCarRows);
    int at = static_cast<int>(IndexOf(focus_));
    for (int tried = 0; tried < count; ++tried) {
        at = (at + step + count) % count;
        const auto row = static_cast<CarRow>(at);
        if (IsEnabled(row)) {
            focus_ = row;
            return;
        }
    }
}

CarRequest EditCarMenu::Choose() const {
    if (!IsEnabled(focus_)) {
        return CarRequest::None;
    }
    switch (focus_) {
        case CarRow::Build:
            return CarRequest::Build;
        case CarRow::RemoveBricks:
            return CarRequest::RemoveBricks;
        case CarRow::QuickBuild:
            return CarRequest::QuickBuild;
        case CarRow::Done:
            return CarRequest::Done;
        case CarRow::Back:
            return CarRequest::Leave;
        case CarRow::PartSet:
            return CarRequest::None;
    }
    return CarRequest::None;
}

}
