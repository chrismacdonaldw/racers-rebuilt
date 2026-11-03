#pragma once

#include <cstddef>

#include "core/duration.hpp"
#include "core/vector3.hpp"

namespace rr::app {

inline constexpr std::size_t kBuildPitches = 3;

class BuildCamera {
public:
    /// False while a turn is still easing.
    bool Turn(int step);

    /// False while a tilt is still easing or the tilt is already at that end.
    bool Tilt(int step);

    /// Turns by part of a slot at once, keeping the slot. False while a turn is still easing.
    bool Swing(float slots);

    /// Tilts by part of a tilt at once, keeping the tilt. False while a tilt is still easing.
    bool Lift(float tilts);

    void Settle(float radius);

    /// Eases back to the opening turn and tilt. False while a turn or tilt is still easing.
    bool Reset();

    void Tick(Milliseconds elapsed);

    [[nodiscard]] std::size_t Slot() const { return slot_; }
    [[nodiscard]] bool IsOverhead() const { return pitch_ + 1 == kBuildPitches; }

    /// Radians the car turns about its up axis.
    [[nodiscard]] float Angle() const;

    [[nodiscard]] Vector3 Eye(float radius) const;

private:
    std::size_t slot_ = 1;
    float place_ = 1.0F;
    float turnFrom_ = 0.0F;
    Milliseconds turnLeft_{0};
    std::size_t pitch_ = 1;
    float tilt_ = 1.0F;
    float tiltFrom_ = 1.0F;
    Milliseconds tiltLeft_{0};
};

}
