#pragma once

#include "assets/race_route.hpp"
#include "core/duration.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/route_playback.hpp"

namespace rr::sim {

/// Unlike `AiRacer`, steers the car physics along the route, drifting where grip can't turn.
class RouteDriver {
public:
    explicit RouteDriver(const assets::RaceRoute& route);

    [[nodiscard]] RacerControls Drive(const Racer& racer);

    /// True once `Drive` has made no headway along the route for 3 s. The count then restarts.
    [[nodiscard]] bool IsStalled(Milliseconds elapsed);

    /// On the route a little ahead of where the driver last followed it, facing along it.
    [[nodiscard]] RacerStart Rejoin() const;

private:
    void Follow(const Vector3& position);

    RoutePlayback nearest_;
    RoutePlayback reference_;
    RoutePlayback ahead_;
    Milliseconds referenceLead_{0};
    Milliseconds aheadLead_{0};
    Milliseconds stalled_{0};
    bool hasHeadway_ = false;
};

}
