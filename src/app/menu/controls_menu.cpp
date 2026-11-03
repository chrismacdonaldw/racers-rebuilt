#include "app/menu/controls_menu.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "app/menu/controls_devices.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kFocusCount = kControlRows + 2;
constexpr std::size_t kUp = kFocusCount - 1;
constexpr std::size_t kDown = 1;
constexpr std::array kSteeringRows{std::size_t{0}, std::size_t{1}};
constexpr std::uint64_t kHashBase = 32;

}

void ControlsMenu::SetEntries(std::vector<InputSet> sets, InputSet set) {
    if (sets.empty()) {
        return;
    }
    sets_ = std::move(sets);
    const auto found = std::ranges::find(sets_, set);
    entry_ = found == sets_.end() ? 0 : static_cast<std::size_t>(found - sets_.begin());
    if (!IsFocusable(focus_)) {
        MoveFocus(kDown);
    }
}

ControlsRequest ControlsMenu::Tick(const sim::InputFrame& input) {
    if (skipsTick_) {
        skipsTick_ = false;
        return ControlsRequest::None;
    }
    if (listening_) {
        return ControlsRequest::None;
    }
    if (input.WasPressed(sim::MenuAction::Up)) {
        MoveFocus(kUp);
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        MoveFocus(kDown);
    }
    if (IsDeviceFocused()) {
        if (input.WasPressed(sim::MenuAction::Left) && entry_ > 0) {
            --entry_;
        }
        if (input.WasPressed(sim::MenuAction::Right) && entry_ + 1 < sets_.size()) {
            ++entry_;
        }
    }
    if (input.WasPressed(sim::MenuAction::Back)) {
        return ControlsRequest::Leave;
    }
    if (input.WasPressed(sim::MenuAction::Confirm)) {
        return Choose();
    }
    return ControlsRequest::None;
}

void ControlsMenu::StopListening() {
    listening_.reset();
    skipsTick_ = true;
}

bool ControlsMenu::IsRowEnabled(std::size_t row) const {
    return RebindsSteering(Set()) || !std::ranges::contains(kSteeringRows, row);
}

std::optional<std::size_t> ControlsMenu::FocusedRow() const {
    if (IsDeviceFocused() || IsFinishFocused()) {
        return std::nullopt;
    }
    return focus_ - 1;
}

std::uint64_t ControlsMenu::StateHash() const {
    std::uint64_t hash = focus_;
    hash = (hash * kHashBase) + entry_;
    hash = (hash * kHashBase) + static_cast<std::uint64_t>(Set());
    return (hash * kHashBase) + (listening_ ? *listening_ + 1 : 0);
}

ControlsRequest ControlsMenu::Choose() {
    if (IsFinishFocused()) {
        return ControlsRequest::Leave;
    }
    const std::optional<std::size_t> row = FocusedRow();
    if (!row) {
        return ControlsRequest::None;
    }
    listening_ = row;
    return ControlsRequest::Listen;
}

bool ControlsMenu::FocusOn(std::size_t focus) {
    if (listening_ || focus >= kFocusCount || !IsFocusable(focus)) {
        return false;
    }
    focus_ = focus;
    return true;
}

bool ControlsMenu::IsFocusable(std::size_t focus) const {
    return focus == 0 || focus > kControlRows || IsRowEnabled(focus - 1);
}

void ControlsMenu::MoveFocus(std::size_t step) {
    do {
        focus_ = (focus_ + step) % kFocusCount;
    } while (!IsFocusable(focus_));
}

}
