#pragma once

#include "core/duration.hpp"
#include "sim/race/random.hpp"

namespace rr::app {

/// Eases back to `base` in 500 ms steps, or swings between 40° and 80° while the racer is cursed.
class FovSwing {
public:
    FovSwing(float baseDegrees, sim::Random random);

    void Tick(Milliseconds elapsed, bool isCursed, bool isGhost);

    [[nodiscard]] float Degrees() const { return degrees_; }

private:
    [[nodiscard]] float SwingAmplitude();

    sim::Random random_;
    float base_;
    float degrees_;
    float start_ = 0.0F;
    float amplitude_ = 0.0F;
    Milliseconds left_{0};
};

}
