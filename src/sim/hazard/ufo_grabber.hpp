#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "assets/hazard_list.hpp"
#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

enum class GrabPhase : std::uint8_t { Free, Holding, Lockout };

struct UfoGrabber {
    std::optional<std::size_t> entity;
    float pull = 0.0F;
    float lowFrame = 0.0F;
    float highFrame = 0.0F;
    BoundingSphere reach;
    GrabPhase phase = GrabPhase::Free;
    std::optional<std::uint8_t> held;
    Milliseconds phaseLeft{0};
    Milliseconds watchdog{0};

    void Tick(HazardContext& context);
    void Touch(HazardContext& context);
    void Deactivate(HazardContext& context) { Release(context); }

private:
    void Grab(std::size_t racer, HazardContext& context);
    void Release(HazardContext& context);
};

[[nodiscard]] UfoGrabber LoadUfoGrabber(const assets::UfoGrabber& payload,
                                        const TrackEntities& entities);

}
