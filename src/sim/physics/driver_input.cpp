#include "sim/physics/driver_input.hpp"

#include <algorithm>
#include <cmath>

namespace rr::sim {
namespace {

constexpr float kHardDriftRate = 16.0F;
constexpr float kDriftRate = 5.0F;
constexpr float kKeyRate = 1.25F;
constexpr float kAxisRate = 2.5F;
constexpr float kReturnRate = 8.25F;
constexpr float kBothPedalsDrive = 0.5F;

float Approach(float value, float target, float step) {
    return value < target ? std::min(value + step, target) : std::max(value - step, target);
}

float TurnRate(const RacerControls& controls, bool isDrifting, bool isKeyHeld) {
    if (isDrifting) {
        return controls.isBrakeHeld ? kHardDriftRate : kDriftRate;
    }
    return isKeyHeld ? kKeyRate : kAxisRate;
}

float SteerToward(float steering, float direction, float rate, float seconds) {
    const float speed = steering * direction < 0.0F ? kReturnRate : rate;
    return steering + (direction * speed * seconds);
}

}

DriveCommand DriverInput::Update(const RacerControls& controls, bool canStartDrift, int dtMs) {
    UpdateSteering(controls, dtMs);
    const float drive = UpdateDrive(controls, canStartDrift);
    return {.steering = steering_,
            .drive = drive,
            .isDrifting = isDrifting_,
            .isHardDrift = isHardDrift_};
}

void DriverInput::EndDrift() {
    isDrifting_ = false;
    isHardDrift_ = false;
}

void DriverInput::UpdateSteering(const RacerControls& controls, int dtMs) {
    const float seconds = static_cast<float>(dtMs) / 1000.0F;
    const bool isKeyHeld = controls.isLeftHeld || controls.isRightHeld;
    const float rate = TurnRate(controls, isDrifting_, isKeyHeld);
    float lower = -1.0F;
    float upper = 1.0F;
    if (controls.isLeftHeld != controls.isRightHeld) {
        steering_ = SteerToward(steering_, controls.isLeftHeld ? 1.0F : -1.0F, rate, seconds);
    } else if (isKeyHeld) {
        steering_ = Approach(steering_, 0.0F, kReturnRate * seconds);
    } else {
        const float axis = std::clamp(controls.steerLeftAxis, -1.0F, 1.0F);
        const float axisRate = rate * (0.5F + (0.5F * (1.0F - std::abs(axis))));
        if (axis > 0.0F) {
            upper = axis;
            steering_ = SteerToward(steering_, 1.0F, axisRate, seconds);
        } else if (axis < 0.0F) {
            lower = axis;
            steering_ = SteerToward(steering_, -1.0F, axisRate, seconds);
        } else {
            steering_ = Approach(steering_, 0.0F, kReturnRate * seconds);
        }
    }
    steering_ = std::clamp(steering_, lower, upper);
}

float DriverInput::UpdateDrive(const RacerControls& controls, bool canStartDrift) {
    const float throttle = controls.isThrottleHeld ? 1.0F : 0.0F;
    const float brake = controls.isBrakeHeld ? -1.0F : 0.0F;
    float drive = throttle + brake;
    if (throttle > 0.0F && brake < 0.0F && !isDrifting_) {
        drive = kBothPedalsDrive;
    }
    if (!controls.isDriftHeld || throttle <= 0.0F) {
        EndDrift();
        return drive;
    }
    if (steering_ == 0.0F) {
        EndDrift();
    } else if (!isDrifting_ && canStartDrift) {
        isDrifting_ = true;
        isHardDrift_ = brake < 0.0F;
    }
    return 1.0F;
}

}
