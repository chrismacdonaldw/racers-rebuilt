#pragma once

#include <cstdint>
#include <optional>

#include "sim/input_frame.hpp"

namespace rr::app {

enum class PauseRow : std::uint8_t { Resume, Restart, Quit, Count };

enum class ConfirmRow : std::uint8_t { Yes, No, Count };

/// Opens on the pause action. Closes on it again, on menu back while pause isn't held, or when
/// Resume or No is confirmed. Restart and Quit ask Yes or No first, starting on No, and the
/// question ignores back and pause.
class PauseMenu {
public:
    /// Returns Restart or Quit on the tick Yes is confirmed for it.
    [[nodiscard]] std::optional<PauseRow> Tick(const sim::InputFrame& input);

    [[nodiscard]] bool IsOpen() const { return isOpen_; }
    [[nodiscard]] PauseRow Focus() const { return focus_; }
    [[nodiscard]] std::optional<ConfirmRow> Confirming() const { return confirm_; }
    [[nodiscard]] std::uint64_t StateHash() const;

private:
    [[nodiscard]] std::optional<PauseRow> TickConfirm(const sim::InputFrame& input);
    void Close();

    bool isOpen_ = false;
    PauseRow focus_ = PauseRow::Resume;
    std::optional<ConfirmRow> confirm_;
};

}
