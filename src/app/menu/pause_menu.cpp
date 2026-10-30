#include "app/menu/pause_menu.hpp"

namespace rr::app {
namespace {

constexpr int kRowCount = static_cast<int>(PauseRow::Count);

}

std::optional<PauseRow> PauseMenu::Tick(const sim::InputFrame& input) {
    if (!isOpen_) {
        isOpen_ = input.WasPressed(sim::Action::Pause);
        focus_ = PauseRow::Resume;
        return std::nullopt;
    }
    if (input.WasPressed(sim::Action::Pause) ||
        (input.WasPressed(sim::MenuAction::Back) && !input.IsHeld(sim::Action::Pause))) {
        isOpen_ = false;
        return std::nullopt;
    }
    if (input.WasPressed(sim::MenuAction::Up)) {
        MoveFocus(-1);
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        MoveFocus(1);
    }
    if (!input.WasPressed(sim::MenuAction::Confirm)) {
        return std::nullopt;
    }
    if (focus_ == PauseRow::Resume) {
        isOpen_ = false;
        return std::nullopt;
    }
    return focus_;
}

std::uint64_t PauseMenu::StateHash() const {
    return isOpen_ ? static_cast<std::uint64_t>(focus_) + 1 : 0;
}

void PauseMenu::MoveFocus(int step) {
    focus_ = static_cast<PauseRow>((static_cast<int>(focus_) + step + kRowCount) % kRowCount);
}

}
