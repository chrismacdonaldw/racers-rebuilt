#pragma once

#include "sim/physics/racer_controls.hpp"

namespace rr::sim {

struct DriveCommand {
    float steering = 0.0F;
    float drive = 0.0F;
    bool isDrifting = false;
    bool isHardDrift = false;
};

class DriverInput {
public:
    [[nodiscard]] DriveCommand Update(const RacerControls& controls, bool canStartDrift, int dtMs);

    void EndDrift();

private:
    void UpdateSteering(const RacerControls& controls, int dtMs);
    [[nodiscard]] float UpdateDrive(const RacerControls& controls, bool canStartDrift);

    float steering_ = 0.0F;
    bool isDrifting_ = false;
    bool isHardDrift_ = false;
};

}
