#pragma once

#include <array>

#include "core/vector3.hpp"

namespace rr::sim {

inline constexpr int kWheelCount = 4;
inline constexpr int kBodyPointCount = 4;

struct ChassisSpec {
    float baseMass = 2500.0F;
    Vector3 centreOfMass{.x = -1.0F, .y = 0.0F, .z = -0.5F};
    float shadowLength = 8.0F;
    float shadowWidth = 9.0F;
    std::array<Vector3, kWheelCount> wheelPositions{{
        {.x = 1.543F, .y = -1.989F, .z = 0.0F},
        {.x = 1.543F, .y = 1.989F, .z = 0.0F},
        {.x = -1.438F, .y = -1.989F, .z = 0.0F},
        {.x = -1.438F, .y = 1.989F, .z = 0.0F},
    }};
    int handling = 33;
    int topSpeed = 33;
    int acceleration = 33;
};

struct CarBuild {
    int highPieceCount = 0;
    float centroidOffsetX = 0.0F;
    float centroidOffsetY = 0.0F;
    float boundsRadius = 5.0F;
};

struct BodyConstants {
    float mass = 0.0F;
    float massScale = 0.0F;
    Vector3 inertia;
    Vector3 centreOfMass;
    Vector3 wheelAnchor;
    float trackWidth = 0.0F;
    float wheelbase = 0.0F;
    std::array<Vector3, kBodyPointCount> bodyPoints{};
    float boundsRadius = 0.0F;
};

struct StatMultipliers {
    float handling = 1.0F;
    float acceleration = 1.0F;
    float topSpeed = 1.0F;
};

[[nodiscard]] BodyConstants MakeBodyConstants(const ChassisSpec& chassis, const CarBuild& build);

[[nodiscard]] StatMultipliers MakeStatMultipliers(const ChassisSpec& chassis,
                                                  const CarBuild& build);

}
