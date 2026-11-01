#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "assets/track_triggers.hpp"
#include "core/bounding_sphere.hpp"
#include "sim/track/track_event.hpp"

namespace rr::sim {

inline constexpr std::size_t kMaxTriggerRacers = 8;

struct TriggerRacer {
    BoundingSphere reach;
    bool isPlayer = false;
    int completedLaps = 0;
};

/// Each racer starts a sphere's event on first touch and ends it a whole tick after leaving.
class RacerTriggers {
public:
    RacerTriggers(std::span<const assets::TriggerSphere> spheres, bool isMirrored);

    void EndFrame(TrackEventQueue& queue);

    /// At most `kMaxTriggerRacers` racers, indexed by their place in `racers`.
    void Test(std::span<const TriggerRacer> racers, TrackEventQueue& queue);

    [[nodiscard]] std::span<const assets::TriggerSphere> Spheres() const { return spheres_; }

    /// One bit per racer that touched `sphere` in the last `Test`.
    [[nodiscard]] std::uint8_t Inside(std::size_t sphere) const { return masks_.at(sphere).inside; }

private:
    struct Masks {
        std::uint8_t inside = 0;
        std::uint8_t wasInside = 0;
    };

    std::vector<assets::TriggerSphere> spheres_;
    std::vector<Masks> masks_;
};

/// Each sphere has one state for every racer and fires at its own centre.
class EnvironmentTriggers {
public:
    EnvironmentTriggers(std::span<const assets::TriggerSphere> spheres, bool isMirrored);

    void EndFrame(TrackEventQueue& queue);
    void Test(std::span<const TriggerRacer> racers, TrackEventQueue& queue);

    [[nodiscard]] std::span<const assets::TriggerSphere> Spheres() const { return spheres_; }
    [[nodiscard]] bool IsActive(std::size_t sphere) const { return states_.at(sphere).isActive; }

private:
    struct State {
        bool isTouched = false;
        bool isActive = false;
    };

    std::vector<assets::TriggerSphere> spheres_;
    std::vector<State> states_;
};

}
