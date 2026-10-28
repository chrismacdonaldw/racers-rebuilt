#include "sim/physics/racer_controls.hpp"

namespace rr::sim {
namespace {

constexpr float kAxisScale = 127.0F;

}

RacerControls ControlsFrom(const InputFrame& input) {
    return {.isLeftHeld = input.IsHeld(Action::SteerLeft),
            .isRightHeld = input.IsHeld(Action::SteerRight),
            .isThrottleHeld = input.IsHeld(Action::Accelerate),
            .isBrakeHeld = input.IsHeld(Action::Brake),
            .isDriftHeld = input.IsHeld(Action::Drift),
            .steerLeftAxis = static_cast<float>(input.steerLeftAxis) / kAxisScale};
}

}
