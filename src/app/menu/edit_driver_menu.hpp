#pragma once

#include <cstdint>
#include <optional>

#include "app/menu/part_carousel.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

enum class DriverRow : std::uint8_t { Hat, Face, Torso, Legs, Mix, Done, Back };

enum class DriverRequest : std::uint8_t { None, Step, Mix, Done, Leave };

class EditDriverMenu {
public:
    DriverRequest Tick(const sim::InputFrame& input);

    [[nodiscard]] DriverRow Focus() const { return focus_; }

    [[nodiscard]] std::optional<DriverSlot> Slot() const;

    /// +1 or -1 for the last step.
    [[nodiscard]] int Direction() const { return direction_; }

private:
    void MoveFocus(int step);

    DriverRow focus_ = DriverRow::Hat;
    int direction_ = 1;
};

}
