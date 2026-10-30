#pragma once

#include <cstdint>
#include <optional>

#include "sim/input_frame.hpp"

namespace rr::app {

enum class PauseRow : std::uint8_t { Resume, Restart, Quit, Count };

/// Opens on the pause action. Closes on it again, on menu back while pause isn't held, or when
/// Resume is confirmed.
class PauseMenu {
public:
    /// Returns Restart or Quit on the tick it is confirmed.
    [[nodiscard]] std::optional<PauseRow> Tick(const sim::InputFrame& input);

    [[nodiscard]] bool IsOpen() const { return isOpen_; }
    [[nodiscard]] PauseRow Focus() const { return focus_; }
    [[nodiscard]] std::uint64_t StateHash() const;

private:
    void MoveFocus(int step);

    bool isOpen_ = false;
    PauseRow focus_ = PauseRow::Resume;
};

}
