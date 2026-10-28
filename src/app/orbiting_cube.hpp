#pragma once

#include "core/transform.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

/// The test scene's cube. Accelerate and brake change how fast it orbits, steering spins it.
class OrbitingCube {
public:
    void Tick(const sim::InputFrame& input);
    [[nodiscard]] Transform Pose() const;

private:
    float orbitRadians_ = 0.0F;
    float spinRadians_ = 0.0F;
};

}
