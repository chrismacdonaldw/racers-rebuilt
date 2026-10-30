#pragma once

#include <optional>

#include "sim/physics/racer_controls.hpp"
#include "sim/physics/turbo.hpp"

namespace rr::sim {

struct StartBoostState {
    bool isHeldAtStart = false;
    int windowMs = 0;
    int heldMs = 0;
    bool isThrottleHeld = false;
};

class StartBoost {
public:
    void HoldAtStart();

    [[nodiscard]] std::optional<TurboLevel> Go();

    [[nodiscard]] std::optional<TurboLevel> Update(const RacerControls& controls, int dtMs);

    [[nodiscard]] const StartBoostState& State() const { return state_; }

private:
    [[nodiscard]] std::optional<TurboLevel> TakeWindow();

    StartBoostState state_;
};

}
