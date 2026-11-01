#pragma once

#include <cstddef>
#include <optional>

#include "assets/hazard_list.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/physics/box_contact.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

struct HazardBox {
    std::optional<std::size_t> entity;
    bool followsNode = false;
    ContactBox box;
    BodyConstants constants;

    void Tick(HazardContext& context);
    void Touch(HazardContext& context) const;
};

/// Moves the entity's clock to the box's start frame.
[[nodiscard]] HazardBox LoadHazardBox(const assets::SwingingBox& payload, TrackEntities& entities);

}
