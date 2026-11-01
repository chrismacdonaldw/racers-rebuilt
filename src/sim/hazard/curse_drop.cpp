#include "sim/hazard/curse_drop.hpp"

#include <array>
#include <cstdint>

#include "core/assert.hpp"
#include "sim/powerup/powerup_system.hpp"

namespace rr::sim {
namespace {

constexpr std::array<Vector3, 3> kDropPoints{{{.x = -357.583F, .y = 458.127F, .z = -11.665F},
                                              {.x = -351.203F, .y = 518.713F, .z = -94.216F},
                                              {.x = -362.258F, .y = 397.304F, .z = -94.960F}}};
constexpr std::uint8_t kCurseLevel = 3;

}

void CurseDrop::Activate(HazardContext& context) {
    RR_ASSERT(context.random != nullptr && context.effects != nullptr && context.aim != nullptr);
    const Vector3& point = kDropPoints.at(context.random->Below(kRollRange) % kDropPoints.size());
    *context.aim = {.source = point, .target = point};
    context.effects->powerups->Fire(
        {.colour = assets::BrickColour::Yellow, .level = kCurseLevel, .hazard = context.slot},
        *context.effects);
}

}
