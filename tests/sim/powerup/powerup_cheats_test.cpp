#include "sim/powerup/powerup_cheats.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>

namespace rr::sim {
namespace {

using assets::BrickColour;

Cheats With(std::initializer_list<Cheat> cheats) {
    unsigned bits = 0;
    for (const Cheat cheat : cheats) {
        bits |= static_cast<unsigned>(cheat);
    }
    return Cheats(static_cast<std::uint16_t>(bits));
}

PowerupUse Use(BrickColour colour, std::uint8_t level) {
    return {.racer = 2, .colour = colour, .level = level};
}

void CheckEveryLevelFiresAs(PowerupKind only) {
    for (std::uint8_t level = 0; level <= kMaxPowerupLevel; ++level) {
        const PowerupUse fired = OnlyUse(only, Use(BrickColour::Red, level));
        CHECK(KindFor(fired.colour, fired.level) == only);
        CHECK(fired.racer == 2);
    }
}

TEST_CASE("bricks keep the track's colours without a brick cheat") {
    CHECK_FALSE(CheatBrickColour(Cheats{}).has_value());
    CHECK_FALSE(CheatBrickColour(With({Cheat::MaxPower})).has_value());
}

TEST_CASE("brick cheats load every brick as one colour, red first") {
    CHECK(CheatBrickColour(With({Cheat::RedBricks})) == BrickColour::Red);
    CHECK(CheatBrickColour(With({Cheat::RedOnly})) == BrickColour::Red);
    CHECK(CheatBrickColour(With({Cheat::YellowBricks})) == BrickColour::Yellow);
    CHECK(CheatBrickColour(With({Cheat::GreenBricks})) == BrickColour::Green);
    CHECK(CheatBrickColour(With({Cheat::GreenBricks, Cheat::YellowBricks})) == BrickColour::Yellow);
}

TEST_CASE("uses fire as held without a use cheat") {
    CHECK(CheatUse(Cheats{}, Use(BrickColour::Blue, 1)) == Use(BrickColour::Blue, 1));
}

TEST_CASE("red only fires every use as a level 1 red") {
    const Cheats redOnly = With({Cheat::RedOnly});

    CHECK(CheatUse(redOnly, Use(BrickColour::Yellow, 3)) == Use(BrickColour::Red, 1));
    CHECK(CheatUse(redOnly, Use(BrickColour::Green, 0)) == Use(BrickColour::Red, 1));
}

TEST_CASE("max power fires every use at level 3") {
    const Cheats maxPower = With({Cheat::MaxPower});

    CHECK(CheatUse(maxPower, Use(BrickColour::Yellow, 0)) == Use(BrickColour::Yellow, 3));
    CHECK(CheatUse(maxPower, Use(BrickColour::Green, 1)) == Use(BrickColour::Green, 3));
}

TEST_CASE("the hover cheat spends a green short of three white bricks without firing") {
    const Cheats hover = With({Cheat::HoverTurbo});

    CHECK_FALSE(CheatUse(hover, Use(BrickColour::Green, 2)).has_value());
    CHECK(CheatUse(hover, Use(BrickColour::Green, 3)) == Use(BrickColour::Green, 3));
    CHECK(CheatUse(hover, Use(BrickColour::Red, 0)) == Use(BrickColour::Red, 0));
    CHECK(CheatUse(With({Cheat::HoverTurbo, Cheat::MaxPower}), Use(BrickColour::Green, 0)) ==
          Use(BrickColour::Green, 3));
}

TEST_CASE("only this power-up fires every use as that kind") {
    for (std::size_t kind = 0; kind < kPowerupKindCount; ++kind) {
        CheckEveryLevelFiresAs(static_cast<PowerupKind>(kind));
    }
    CHECK(OnlyUse(PowerupKind::Shield, Use(BrickColour::Red, 2)) == Use(BrickColour::Blue, 2));
    CHECK(OnlyUse(PowerupKind::Turbo, Use(BrickColour::Red, 1)) == Use(BrickColour::Green, 1));
}

TEST_CASE("red only still wins over max power when both are on") {
    const Cheats both = With({Cheat::RedOnly, Cheat::MaxPower});

    CHECK(CheatUse(both, Use(BrickColour::Yellow, 0)) == Use(BrickColour::Red, 1));
}

}
}
