#pragma once

#include <cstdint>
#include <optional>

#include "assets/race_route.hpp"
#include "core/duration.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/route_playback.hpp"

namespace rr::sim {

/// Unlike `AiRacer`, steers the car physics along the route, drifting where grip can't turn.
class RouteDriver {
public:
    explicit RouteDriver(const assets::RaceRoute& route, std::optional<float> pace = std::nullopt);

    /// Paced, coasts above its share of the route's recorded speed and brakes well above it.
    [[nodiscard]] RacerControls Drive(const Racer& racer);

    /// True once `Drive` has made no headway along the route for 3 s. The count then restarts.
    [[nodiscard]] bool IsStalled(Milliseconds elapsed);

    /// On the route a little ahead of where the driver last followed it, facing along it.
    [[nodiscard]] RacerStart Rejoin() const;

    [[nodiscard]] const RoutePlayback& Nearest() const { return nearest_; }
    [[nodiscard]] const RoutePlayback& Reference() const { return reference_; }
    [[nodiscard]] const RoutePlayback& Ahead() const { return ahead_; }
    [[nodiscard]] Milliseconds ReferenceLead() const { return referenceLead_; }
    [[nodiscard]] Milliseconds AheadLead() const { return aheadLead_; }
    [[nodiscard]] Milliseconds StalledFor() const { return stalled_; }
    [[nodiscard]] bool HasHeadway() const { return hasHeadway_; }

private:
    void Follow(const Vector3& position);
    enum class Pedal : std::uint8_t { Throttle, Coast, Brake };

    [[nodiscard]] Pedal PedalFor(const BodyMotion& motion) const;

    RoutePlayback nearest_;
    RoutePlayback reference_;
    RoutePlayback ahead_;
    std::optional<float> pace_;
    Milliseconds referenceLead_{0};
    Milliseconds aheadLead_{0};
    Milliseconds stalled_{0};
    bool hasHeadway_ = false;
};

}
