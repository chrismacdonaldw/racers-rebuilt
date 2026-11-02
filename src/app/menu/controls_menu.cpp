#include "app/menu/controls_menu.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kFocusCount = kControlRows + 2;
constexpr std::uint64_t kHashBase = 32;

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
        focus_ = (focus_ + kFocusCount - 1) % kFocusCount;
    }
    if (input.WasPressed(sim::MenuAction::Down)) {
        focus_ = (focus_ + 1) % kFocusCount;
    }
    if (IsDeviceFocused()) {
        if (input.WasPressed(sim::MenuAction::Left)) {
            device_ = ControlsDevice::Keyboard;
        }
        if (input.WasPressed(sim::MenuAction::Right)) {
            device_ = ControlsDevice::Gamepad;
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

std::optional<std::size_t> ControlsMenu::FocusedRow() const {
    if (IsDeviceFocused() || IsFinishFocused()) {
        return std::nullopt;
    }
    return focus_ - 1;
}

std::uint64_t ControlsMenu::StateHash() const {
    std::uint64_t hash = focus_;
    hash = (hash * kHashBase) + static_cast<std::uint64_t>(device_);
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

}
