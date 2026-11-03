#pragma once

#include "sim/physics/racer_controls.hpp"
#include "sim/physics/version_rules.hpp"

namespace rr::sim {

struct DriveCommand {
    float steering = 0.0F;
    float drive = 0.0F;
    bool isDrifting = false;
    bool isHardDrift = false;
};

struct DriverState {
    float steering = 0.0F;
    bool isDrifting = false;
    bool isHardDrift = false;
};

class DriverInput {
public:
    [[nodiscard]] DriveCommand Update(const RacerControls& controls, bool canStartDrift,
                                      const VersionRules& rules, int dtMs);

    void EndDrift();

    [[nodiscard]] const DriverState& State() const { return state_; }

private:
    void UpdateSteering(const RacerControls& controls, const VersionRules& rules, int dtMs);
    [[nodiscard]] float UpdateDrive(const RacerControls& controls, bool canStartDrift);

    DriverState state_;
};

}
