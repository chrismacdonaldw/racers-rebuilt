#include "sim/powerup/powerup_kind.hpp"

#include <array>

#include "core/assert.hpp"

namespace rr::sim {
namespace {

constexpr std::array<PowerupKindSpec, kPowerupKindCount> kSpecs{{
    {.poolSize = 4},
    {.poolSize = 3},
    {.poolSize = 3},
    {.poolSize = 9, .launches = 3},
    {.poolSize = 4},
    {.poolSize = 3},
    {.poolSize = 3, .entities = 3},
    {.poolSize = 2, .entities = 3},
    {.poolSize = 5, .entities = 2},
    {.poolSize = 6, .entities = 3},
    {.poolSize = 3},
}};

}

PowerupKind KindFor(assets::BrickColour colour, std::uint8_t level, const PowerupSlots& slots) {
    RR_ASSERT(level <= kMaxPowerupLevel);
    return slots.at(static_cast<std::size_t>(colour)).at(level);
}

const PowerupKindSpec& SpecFor(PowerupKind kind) {
    return kSpecs.at(static_cast<std::size_t>(kind));
}

}
