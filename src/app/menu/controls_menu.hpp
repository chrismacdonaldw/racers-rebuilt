#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "app/device_assignment.hpp"
#include "app/menu/control_bindings.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

enum class ControlsRequest : std::uint8_t { None, Listen, Leave };

/// While it listens for a new input, menu actions do nothing.
class ControlsMenu {
public:
    /// Shows `set` when it's among `sets`, else the first.
    void SetEntries(std::vector<InputSet> sets, InputSet set);

    [[nodiscard]] ControlsRequest Tick(const sim::InputFrame& input);

    /// Also ignores the next tick's menu actions, which the same press latched.
    void StopListening();

    [[nodiscard]] InputSet Set() const { return sets_.at(entry_); }
    [[nodiscard]] std::size_t Entry() const { return entry_; }
    [[nodiscard]] bool IsDeviceFocused() const { return focus_ == 0; }
    [[nodiscard]] bool IsFinishFocused() const { return focus_ == kControlRows + 1; }
    [[nodiscard]] bool IsRowEnabled(std::size_t row) const;
    [[nodiscard]] std::optional<std::size_t> FocusedRow() const;
    [[nodiscard]] std::optional<std::size_t> ListeningRow() const { return listening_; }
    [[nodiscard]] std::uint64_t StateHash() const;

private:
    [[nodiscard]] ControlsRequest Choose();
    [[nodiscard]] bool IsFocusable(std::size_t focus) const;
    void MoveFocus(std::size_t step);

    std::vector<InputSet> sets_{InputSet::Keys1, InputSet::Keys2, InputSet::Keys3};
    std::size_t entry_ = 0;
    std::size_t focus_ = 1;
    std::optional<std::size_t> listening_;
    bool skipsTick_ = false;
};

}
