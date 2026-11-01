#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "sim/collision/track_surfaces.hpp"
#include "sim/physics/racer.hpp"
#include "sim/track/track_events.hpp"

namespace rr::sim {

using WheelSurfaces = std::array<std::optional<std::size_t>, kWheelCount>;

/// Fires each event on both paths: at the wheel or touch point, and for the racer.
class SurfaceEvents {
public:
    explicit SurfaceEvents(std::size_t racerCount);

    void Hear(std::uint8_t racer, const Racer& car, const TrackSurfaces& surfaces,
              TrackEvents& events);

    [[nodiscard]] const WheelSurfaces& Under(std::size_t racer) const { return under_.at(racer); }

    [[nodiscard]] static std::size_t MostEventsPerRacer();

private:
    std::vector<WheelSurfaces> under_;
};

}
