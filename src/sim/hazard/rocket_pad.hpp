#pragma once

#include <cstddef>
#include <optional>

#include "assets/event_id.hpp"
#include "core/bounding_sphere.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/model_switches.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

inline constexpr assets::EventId kRocketPadTrigger = 1;
inline constexpr assets::EventId kRocketPadEvent = 35;

struct RocketPad {
    std::optional<BoundingSphere> reach;
    std::size_t offModel = 0;
    std::size_t onModel = 0;
    bool isOn = false;
    bool hasShieldedRacer = false;

    void Tick(HazardContext& context);
    void Touch(HazardContext& context);
    void Deactivate(HazardContext& context);

private:
    void Switch(bool on, HazardContext& context);
};

[[nodiscard]] RocketPad LoadRocketPad(const TrackEntities& entities, ModelSwitches& models);

}
