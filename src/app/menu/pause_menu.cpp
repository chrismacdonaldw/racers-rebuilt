#include "app/menu/pause_menu.hpp"

namespace rr::app {
namespace {

constexpr int kConfirmShift = 8;

template <typename Row>
Row Stepped(Row row, const sim::InputFrame& input) {
    constexpr int kCount = static_cast<int>(Row::Count);
    const int step = (input.WasPressed(sim::MenuAction::Down) ? 1 : 0) -
                     (input.WasPressed(sim::MenuAction::Up) ? 1 : 0);
    return static_cast<Row>((static_cast<int>(row) + step + kCount) % kCount);
}

}

std::optional<PauseRow> PauseMenu::Tick(const sim::InputFrame& input) {
    if (!isOpen_) {
        isOpen_ = input.WasAnyPressed(sim::Action::Pause);
        focus_ = PauseRow::Resume;
        return std::nullopt;
    }
    if (confirm_) {
        return TickConfirm(input);
    }
    if (input.WasAnyPressed(sim::Action::Pause) ||
        (input.WasPressed(sim::MenuAction::Back) && !input.IsAnyHeld(sim::Action::Pause))) {
        Close();
        return std::nullopt;
    }
    focus_ = Stepped(focus_, input);
    if (!input.WasPressed(sim::MenuAction::Confirm)) {
        return std::nullopt;
    }
    if (focus_ == PauseRow::Resume) {
        Close();
    } else {
        confirm_ = ConfirmRow::No;
    }
    return std::nullopt;
}

std::optional<PauseRow> PauseMenu::TickConfirm(const sim::InputFrame& input) {
    confirm_ = Stepped(*confirm_, input);
    if (!input.WasPressed(sim::MenuAction::Confirm)) {
        return std::nullopt;
    }
    if (confirm_ == ConfirmRow::No) {
        Close();
        return std::nullopt;
    }
    return focus_;
}

void PauseMenu::Close() {
    isOpen_ = false;
    confirm_.reset();
}

std::uint64_t PauseMenu::StateHash() const {
    if (!isOpen_) {
        return 0;
    }
    const std::uint64_t confirm = confirm_ ? static_cast<std::uint64_t>(*confirm_) + 1 : 0;
    return (static_cast<std::uint64_t>(focus_) + 1) | (confirm << kConfirmShift);
}

}
