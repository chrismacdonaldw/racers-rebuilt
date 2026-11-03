#include "sim/ai/ai_aim.hpp"

#include <doctest/doctest.h>

#include <array>
#include <optional>
#include <span>

namespace rr::sim {
namespace {

using assets::BrickColour;

TEST_CASE("a red brick aims at the nearest rival in the cone ahead") {
    const std::array field{Vector3{}, Vector3{.x = 120.0F, .y = 10.0F},
                           Vector3{.x = 60.0F, .y = -20.0F}, Vector3{.x = 30.0F, .y = 40.0F},
                           Vector3{.x = -40.0F}};

    CHECK(AimFor(BrickColour::Red, field, 0, BodyMotion{}) == field[2]);
}

TEST_CASE("a yellow brick aims at the nearest rival in the cone behind") {
    const std::array field{Vector3{}, Vector3{.x = 40.0F}, Vector3{.x = -80.0F, .y = 5.0F},
                           Vector3{.x = -30.0F, .y = 10.0F}, Vector3{.x = -3.0F}};

    CHECK(AimFor(BrickColour::Yellow, field, 0, BodyMotion{}) == field[3]);
}

TEST_CASE("a brick never aims at its own racer's stale position") {
    const std::array field{Vector3{.x = -20.0F}, Vector3{.x = -60.0F}};

    CHECK(AimFor(BrickColour::Yellow, field, 0, BodyMotion{}) == field[1]);
    CHECK_FALSE(
        AimFor(BrickColour::Yellow, std::span(field).first(1), 0, BodyMotion{}).has_value());
}

TEST_CASE("a brick aims nowhere past the cone's reach or without a direction") {
    const std::array field{Vector3{}, Vector3{.x = 300.0F}, Vector3{.x = -120.0F}};
    const std::array near{Vector3{}, Vector3{.x = 50.0F}, Vector3{.x = -50.0F}};

    CHECK_FALSE(AimFor(BrickColour::Red, field, 0, BodyMotion{}).has_value());
    CHECK_FALSE(AimFor(BrickColour::Yellow, field, 0, BodyMotion{}).has_value());
    CHECK_FALSE(AimFor(BrickColour::Green, near, 0, BodyMotion{}).has_value());
    CHECK_FALSE(AimFor(BrickColour::Blue, near, 0, BodyMotion{}).has_value());
    CHECK_FALSE(AimFor(std::nullopt, near, 0, BodyMotion{}).has_value());
}

}
}
