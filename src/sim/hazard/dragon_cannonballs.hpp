#pragma once

#include <cstddef>
#include <optional>

#include "assets/hazard_list.hpp"
#include "core/duration.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

struct DragonCannonballs {
    std::optional<std::size_t> entity;
    Milliseconds interval{0};
    Milliseconds wait{0};

    void Activate() { wait = interval; }
    void Tick(HazardContext& context);
};

[[nodiscard]] DragonCannonballs LoadDragonCannonballs(const assets::DragonCannonballs& payload,
                                                      const TrackEntities& entities);

}
