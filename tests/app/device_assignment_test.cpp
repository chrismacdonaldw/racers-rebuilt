#include "app/device_assignment.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

using enum InputSet;

TEST_CASE("out of the box players take the pads, then the arrows, then WASD") {
    CHECK(AssignInputSets(kDefaultPlayerSets, 2) == PlayerSets{Pad1, Pad2});
    CHECK(AssignInputSets(kDefaultPlayerSets, 1) == PlayerSets{Pad1, Keys1});
    CHECK(AssignInputSets(kDefaultPlayerSets, 0) == PlayerSets{Keys1, Keys2});
}

TEST_CASE("a set naming a missing pad falls back to the first attached pad not in use") {
    CHECK(AssignInputSets({Pad2, Keys1}, 1) == PlayerSets{Pad1, Keys1});
    CHECK(AssignInputSets({Keys3, Pad2}, 1) == PlayerSets{Keys3, Pad1});
    CHECK(AssignInputSets({Pad1, Pad2}, 3) == PlayerSets{Pad1, Pad2});
}

TEST_CASE("with no pad free the fallback avoids the other player's arrows") {
    CHECK(AssignInputSets({Pad1, Keys1}, 0) == PlayerSets{Keys2, Keys1});
    CHECK(AssignInputSets({Keys2, Pad1}, 0) == PlayerSets{Keys2, Keys1});
    CHECK(AssignInputSets({Pad1, Pad1}, 1) == PlayerSets{Pad1, Keys1});
}

TEST_CASE("the second player moves off a set the first player already has") {
    CHECK(AssignInputSets({Keys2, Keys2}, 0) == PlayerSets{Keys2, Keys1});
    CHECK(AssignInputSets({Keys1, Keys1}, 2) == PlayerSets{Keys1, Pad1});
}

TEST_CASE("choosing the other player's set moves them by the fallback") {
    CHECK(ChooseInputSet({Pad1, Keys1}, 0, Keys1, 1) == PlayerSets{Keys1, Pad1});
    CHECK(ChooseInputSet({Pad1, Keys1}, 1, Pad1, 1) == PlayerSets{Keys1, Pad1});
    CHECK(ChooseInputSet({Keys1, Keys2}, 1, Keys1, 0) == PlayerSets{Keys2, Keys1});
    CHECK(ChooseInputSet({Pad1, Pad2}, 0, Pad2, 2) == PlayerSets{Pad2, Pad1});
}

TEST_CASE("choosing a free set leaves the other player alone") {
    CHECK(ChooseInputSet({Pad1, Keys1}, 1, Keys3, 1) == PlayerSets{Pad1, Keys3});
}

TEST_CASE("set names read back") {
    for (const InputSet set : kInputSets) {
        CHECK(ParseInputSet(InputSetName(set)) == set);
    }
    CHECK_FALSE(ParseInputSet("pad3").has_value());
    CHECK_FALSE(ParseInputSet("").has_value());
}

TEST_CASE("pads number from zero") {
    CHECK(PadOf(Pad2) == 1);
    CHECK_FALSE(PadOf(Keys1).has_value());
}

}
}
