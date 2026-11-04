#include "app/menu/garage_menu.hpp"

#include <algorithm>
#include <array>
#include <iterator>

namespace rr::app {
namespace {

constexpr std::array kMainButtons{GarageButton::New,       GarageButton::Edit,
                                  GarageButton::Copy,      GarageButton::Delete,
                                  GarageButton::TestDrive, GarageButton::Back};
constexpr std::array kEditButtons{GarageButton::Driver, GarageButton::Licence, GarageButton::Car,
                                  GarageButton::Back};
constexpr std::uint64_t kButtonBase = 16;

}

GarageMenu::GarageMenu(std::size_t racers) : racers_(racers) {
    Open(false);
}

GarageRequest GarageMenu::Tick(const sim::InputFrame& input) {
    if (input.WasPressed(sim::MenuAction::Back)) {
        if (isEditing_) {
            Open(false);
            focus_ = GarageButton::Edit;
            return GarageRequest::Page;
        }
        return GarageRequest::Leave;
    }
    if (input.WasPressed(sim::MenuAction::Up)) {
        MoveFocus(-1);
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        MoveFocus(1);
    }
    if (!isEditing_ && input.WasPressed(sim::MenuAction::Left)) {
        return Step(racers_ - 1);
    }
    if (!isEditing_ && input.WasPressed(sim::MenuAction::Right)) {
        return Step(1);
    }
    return input.WasPressed(sim::MenuAction::Confirm) ? Choose() : GarageRequest::None;
}

void GarageMenu::Refresh(std::size_t racers, std::size_t racer, GarageButton focus) {
    racers_ = racers;
    racer_ = racers == 0 ? 0 : std::min(racer, racers - 1);
    if (racers == 0 && isEditing_) {
        Open(false);
        return;
    }
    focus_ = focus;
    if (std::ranges::find(Buttons(), focus) == Buttons().end() || !IsEnabled(focus)) {
        Open(isEditing_);
    }
}

void GarageMenu::LockTestDrive(bool isLocked) {
    isTestDriveLocked_ = isLocked;
    if (isLocked && focus_ == GarageButton::TestDrive) {
        MoveFocus(1);
    }
}

bool GarageMenu::FocusOn(GarageButton button) {
    if (!std::ranges::contains(Buttons(), button) || !IsEnabled(button)) {
        return false;
    }
    focus_ = button;
    return true;
}

std::span<const GarageButton> GarageMenu::Buttons() const {
    if (isEditing_) {
        return kEditButtons;
    }
    return kMainButtons;
}

bool GarageMenu::IsEnabled(GarageButton button) const {
    switch (button) {
        case GarageButton::New:
            return racers_ < kGarageCapacity;
        case GarageButton::Copy:
            return racers_ != 0 && racers_ < kGarageCapacity;
        case GarageButton::Edit:
        case GarageButton::Delete:
            return racers_ != 0;
        case GarageButton::TestDrive:
            return racers_ != 0 && !isTestDriveLocked_;
        default:
            return true;
    }
}

std::uint64_t GarageMenu::StateHash() const {
    const std::uint64_t page = (racer_ * 4) + (isEditing_ ? 2 : 0) + (isTestDriveLocked_ ? 1 : 0);
    return (page * kButtonBase) + static_cast<std::uint64_t>(focus_);
}

void GarageMenu::MoveFocus(int step) {
    const std::span<const GarageButton> buttons = Buttons();
    const auto count = static_cast<std::ptrdiff_t>(buttons.size());
    std::ptrdiff_t at = std::distance(buttons.begin(), std::ranges::find(buttons, focus_));
    for (std::ptrdiff_t tried = 0; tried < count; ++tried) {
        at = (at + step + count) % count;
        if (IsEnabled(buttons[static_cast<std::size_t>(at)])) {
            focus_ = buttons[static_cast<std::size_t>(at)];
            return;
        }
    }
}

void GarageMenu::Open(bool isEditing) {
    isEditing_ = isEditing;
    if (isEditing) {
        focus_ = GarageButton::Driver;
        return;
    }
    focus_ = IsEnabled(GarageButton::New) ? GarageButton::New : GarageButton::Edit;
}

GarageRequest GarageMenu::Step(std::size_t forward) {
    if (racers_ < 2) {
        return GarageRequest::None;
    }
    racer_ = (racer_ + forward) % racers_;
    return GarageRequest::Step;
}

GarageRequest GarageMenu::Choose() {
    if (!IsEnabled(focus_)) {
        return GarageRequest::None;
    }
    switch (focus_) {
        case GarageButton::New:
            return GarageRequest::New;
        case GarageButton::Edit:
            Open(true);
            return GarageRequest::Page;
        case GarageButton::Copy:
            return GarageRequest::Copy;
        case GarageButton::Delete:
            return GarageRequest::Delete;
        case GarageButton::TestDrive:
            return GarageRequest::TestDrive;
        case GarageButton::Driver:
            return GarageRequest::Driver;
        case GarageButton::Licence:
            return GarageRequest::Licence;
        case GarageButton::Car:
            return GarageRequest::Car;
        case GarageButton::Back:
            if (!isEditing_) {
                return GarageRequest::Leave;
            }
            Open(false);
            focus_ = GarageButton::Edit;
            return GarageRequest::Page;
    }
    return GarageRequest::None;
}

}
