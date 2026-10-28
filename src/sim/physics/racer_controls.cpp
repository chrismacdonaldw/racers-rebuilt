#include "sim/physics/racer_controls.hpp"

namespace rr::sim {

RacerControls ControlsFrom(const InputFrame& input) {
    return {.isLeftHeld = input.IsHeld(Action::SteerLeft),
            .isRightHeld = input.IsHeld(Action::SteerRight),
            .isThrottleHeld = input.IsHeld(Action::Accelerate),
            .isBrakeHeld = input.IsHeld(Action::Brake),
            .isDriftHeld = input.IsHeld(Action::Drift),
            .steerLeftAxis = 0.0F};
}

}
