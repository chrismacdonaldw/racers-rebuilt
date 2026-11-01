#pragma once

#include "assets/event_id.hpp"
#include "sim/hazard/hazard_context.hpp"

namespace rr::sim {

inline constexpr assets::EventId kCurseDropTrigger = 8;

struct CurseDrop {
    static void Activate(HazardContext& context);
};

}
