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

    void Tick(Milliseconds elapsed);

    [[nodiscard]] std::size_t Slot() const { return slot_; }
    [[nodiscard]] bool IsOverhead() const { return pitch_ + 1 == kBuildPitches; }

    /// Radians the car turns about its up axis.
    [[nodiscard]] float Angle() const;

    [[nodiscard]] Vector3 Eye(float radius) const;

private:
    std::size_t slot_ = 1;
    float turnFrom_ = 0.0F;
    Milliseconds turnLeft_{0};
    std::size_t pitch_ = 1;
    std::size_t tiltFrom_ = 1;
    Milliseconds tiltLeft_{0};
};

}
