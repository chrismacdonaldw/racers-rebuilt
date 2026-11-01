#include "sim/hazard/warp_pad.hpp"

#include <cstdint>

#include "core/assert.hpp"
#include "sim/powerup/powerup_system.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kTarget{.x = 132.673F, .y = 86.304F, .z = 14.722F};
constexpr Vector3 kHeading{.x = 1.0F, .y = -0.5F};
constexpr std::uint8_t kWarpLevel = 3;

}

void WarpPad::Hear(const TrackEvent& event, HazardContext& context) {
    if (event.id != kWarpPadEvent || !event.racer || event.edge != EventEdge::Start) {
        return;
    }
    RR_ASSERT(context.effects != nullptr && context.aim != nullptr);
    const RacerStatus& status = context.racers[*event.racer].Status();
    if (status.isGhost || status.isWarping) {
        return;
    }
    *context.aim = {
        .source = kTarget, .target = kTarget, .direction = Normalized(kHeading, kHeading)};
    context.effects->powerups->Fire({.racer = *event.racer,
                                     .colour = assets::BrickColour::Green,
                                     .level = kWarpLevel,
                                     .hazard = context.slot},
                                    *context.effects);
}

}
