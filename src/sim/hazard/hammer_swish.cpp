#include "sim/hazard/hammer_swish.hpp"

#include "sim/hazard/swing_strike.hpp"

namespace rr::sim {
namespace {

constexpr StrikeWindows kWindows{
    .strike = {{{.after = 22.0F, .before = 28.0F}, {.after = 72.0F, .before = 78.0F}}},
    .rearm = {{{.after = 0.0F, .before = 20.0F}, {.after = 40.0F, .before = 60.0F}}}};

}

void HammerSwish::Tick(HazardContext& context) {
    if (entity && Swing(kWindows, context.entities->Frame(*entity), isArmed)) {
        context.events->FireStartAndEnd(kHammerSwishEvent, std::nullopt);
    }
}

HammerSwish LoadHammerSwish(const TrackEntities& entities) {
    return {.entity = entities.Find("rkhamm02")};
}

}
