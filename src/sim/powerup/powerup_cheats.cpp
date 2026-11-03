#include "sim/powerup/powerup_cheats.hpp"

namespace rr::sim {
namespace {

constexpr std::uint8_t kRedOnlyLevel = 1;

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

}
