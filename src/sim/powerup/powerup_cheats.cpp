#include "sim/powerup/powerup_cheats.hpp"

#include <algorithm>
#include <cstdint>

namespace rr::sim {
namespace {

constexpr std::uint8_t kRedOnlyLevel = 1;
constexpr std::uint8_t kLastTurboLevel = kMaxPowerupLevel - 1;

}

std::optional<assets::BrickColour> CheatBrickColour(const Cheats& cheats) {
    if (cheats.Has(Cheat::RedBricks) || cheats.Has(Cheat::RedOnly)) {
        return assets::BrickColour::Red;
    }
    if (cheats.Has(Cheat::YellowBricks)) {
        return assets::BrickColour::Yellow;
    }
    if (cheats.Has(Cheat::GreenBricks)) {
        return assets::BrickColour::Green;
    }
    return std::nullopt;
}

std::optional<PowerupUse> CheatUse(const Cheats& cheats, PowerupUse use) {
    if (cheats.Has(Cheat::RedOnly)) {
        use.colour = assets::BrickColour::Red;
        use.level = kRedOnlyLevel;
        return use;
    }
    if (cheats.Has(Cheat::MaxPower)) {
        use.level = kMaxPowerupLevel;
    }
    if (cheats.Has(Cheat::HoverTurbo) && use.colour == assets::BrickColour::Green &&
        use.level != kMaxPowerupLevel) {
        return std::nullopt;
    }
    return use;
}

PowerupUse OnlyUse(PowerupKind kind, PowerupUse use) {
    const auto levelFrom = [kind](PowerupKind first) {
        return static_cast<std::uint8_t>(static_cast<int>(kind) - static_cast<int>(first));
    };
    switch (kind) {
        case PowerupKind::Cannonball:
        case PowerupKind::GrapplingHook:
        case PowerupKind::Lightning:
        case PowerupKind::Missile:
            use.colour = assets::BrickColour::Red;
            use.level = levelFrom(PowerupKind::Cannonball);
            break;
        case PowerupKind::OilSlick:
        case PowerupKind::Dynamite:
        case PowerupKind::Magnet:
        case PowerupKind::Curse:
            use.colour = assets::BrickColour::Yellow;
            use.level = levelFrom(PowerupKind::OilSlick);
            break;
        case PowerupKind::Shield:
            use.colour = assets::BrickColour::Blue;
            break;
        case PowerupKind::Turbo:
            use.colour = assets::BrickColour::Green;
            use.level = std::min(use.level, kLastTurboLevel);
            break;
        case PowerupKind::Warp:
        case PowerupKind::Count:
            use.colour = assets::BrickColour::Green;
            use.level = kMaxPowerupLevel;
            break;
    }
    return use;
}

}
