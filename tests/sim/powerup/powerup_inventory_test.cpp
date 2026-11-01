#include "sim/powerup/powerup_inventory.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <optional>

namespace rr::sim {
namespace {

using assets::BrickColour;

TEST_CASE("taking a colour replaces the one held and hands it back") {
    PowerupInventory inventory;

    CHECK_FALSE(inventory.TakeColour(BrickColour::Red).has_value());
    CHECK(inventory.TakeColour(BrickColour::Blue) == BrickColour::Red);
    CHECK(inventory.Colour() == BrickColour::Blue);
}

TEST_CASE("a racer holds at most three white bricks and keeps them through a colour change") {
    PowerupInventory inventory;

    CHECK(inventory.TakeWhite(4));
    CHECK(inventory.TakeWhite(5));
    CHECK(inventory.TakeWhite(6));
    CHECK_FALSE(inventory.TakeWhite(7));
    inventory.TakeColour(BrickColour::Green);

    CHECK(inventory.WhiteCount() == 3);
}

TEST_CASE("losing a white brick frees its slot, and the next pickup fills it first") {
    PowerupInventory inventory;
    CHECK(inventory.TakeWhite(4));
    CHECK(inventory.TakeWhite(5));

    CHECK(inventory.LoseWhite() == 4);
    CHECK(inventory.TakeWhite(6));

    CHECK(inventory.WhiteCount() == 2);
    CHECK(inventory.LoseWhite() == 6);
    CHECK(inventory.LoseWhite() == 5);
    CHECK_FALSE(inventory.LoseWhite().has_value());
}

TEST_CASE("spending empties the inventory and returns the white bricks held") {
    PowerupInventory inventory;
    inventory.TakeColour(BrickColour::Yellow);
    CHECK(inventory.TakeWhite(2));

    const std::array<std::optional<std::size_t>, kMaxWhiteBricks> spent = inventory.Spend();

    CHECK(spent[0] == 2);
    CHECK_FALSE(spent[1].has_value());
    CHECK(inventory == PowerupInventory{});
}

}
TEST_CASE("a grant holds white bricks that belong to no brick, up to the cap") {
    PowerupInventory inventory;

    inventory.Grant(BrickColour::Red, 2);

    CHECK(inventory.Colour() == BrickColour::Red);
    CHECK(inventory.WhiteCount() == 2);
    CHECK(inventory.TakeWhite(9));
    CHECK_FALSE(inventory.TakeWhite(10));
    CHECK(inventory.WhiteCount() == 3);
}

TEST_CASE("a racer loses real white bricks before granted ones") {
    PowerupInventory inventory;
    inventory.Grant(BrickColour::Blue, 1);
    CHECK(inventory.TakeWhite(4));

    CHECK(inventory.LoseWhite() == 4);
    CHECK_FALSE(inventory.LoseWhite().has_value());
    CHECK(inventory.WhiteCount() == 0);
    CHECK(inventory.Spend() == std::array<std::optional<std::size_t>, kMaxWhiteBricks>{});
    CHECK(inventory == PowerupInventory{});
}

}
