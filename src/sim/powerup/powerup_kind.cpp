#include "sim/powerup/powerup_kind.hpp"

#include <array>

#include "core/assert.hpp"

namespace rr::sim {
namespace {

constexpr std::array kRedKinds{PowerupKind::Cannonball, PowerupKind::GrapplingHook,
                               PowerupKind::Lightning, PowerupKind::Missile};
constexpr std::array kYellowKinds{PowerupKind::OilSlick, PowerupKind::Dynamite, PowerupKind::Magnet,
                                  PowerupKind::Curse};

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

PowerupKind KindFor(assets::BrickColour colour, std::uint8_t level) {
    RR_ASSERT(level <= kMaxPowerupLevel);
    switch (colour) {
        case assets::BrickColour::Red:
            return kRedKinds.at(level);
        case assets::BrickColour::Yellow:
            return kYellowKinds.at(level);
        case assets::BrickColour::Blue:
            return PowerupKind::Shield;
        case assets::BrickColour::Green:
            return level == kMaxPowerupLevel ? PowerupKind::Warp : PowerupKind::Turbo;
    }
    return PowerupKind::Turbo;
}

const PowerupKindSpec& SpecFor(PowerupKind kind) {
    return kSpecs.at(static_cast<std::size_t>(kind));
}

}
