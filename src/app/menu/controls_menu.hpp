#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "app/menu/control_bindings.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

enum class ControlsRequest : std::uint8_t { None, Listen, Leave };

/// While it listens for a new input, menu actions do nothing.
class ControlsMenu {
public:
    [[nodiscard]] ControlsRequest Tick(const sim::InputFrame& input);

    /// Also ignores the next tick's menu actions, which the same press latched.
    void StopListening();

    [[nodiscard]] ControlsDevice Device() const { return device_; }
    [[nodiscard]] bool IsDeviceFocused() const { return focus_ == 0; }
    [[nodiscard]] bool IsFinishFocused() const { return focus_ == kControlRows + 1; }
    [[nodiscard]] std::optional<std::size_t> FocusedRow() const;
    [[nodiscard]] std::optional<std::size_t> ListeningRow() const { return listening_; }
    [[nodiscard]] std::uint64_t StateHash() const;

private:
    [[nodiscard]] ControlsRequest Choose();

    std::size_t focus_ = 1;
    ControlsDevice device_ = ControlsDevice::Keyboard;
    std::optional<std::size_t> listening_;
    bool skipsTick_ = false;
};

}
