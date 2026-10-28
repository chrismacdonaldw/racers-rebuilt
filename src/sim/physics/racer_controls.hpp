#pragma once

#include "sim/input_frame.hpp"

namespace rr::sim {

struct RacerControls {
    bool isLeftHeld = false;
    bool isRightHeld = false;
    bool isThrottleHeld = false;
    bool isBrakeHeld = false;
    bool isDriftHeld = false;
    float steerLeftAxis = 0.0F;

    friend constexpr bool operator==(const RacerControls&, const RacerControls&) = default;
};

[[nodiscard]] RacerControls ControlsFrom(const InputFrame& input);

}
