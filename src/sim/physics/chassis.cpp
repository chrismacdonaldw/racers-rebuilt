#include "sim/physics/chassis.hpp"

#include <algorithm>

namespace rr::sim {
namespace {

constexpr float kBoxLength = 8.0F;
constexpr float kBoxWidth = 5.0F;
constexpr float kBoxHeight = 6.2F;
constexpr float kBodyPointHeight = 3.5F;
constexpr float kStatPerPiece = 0.05F;

Vector3 BoxInertia(float mass) {
    const float length2 = kBoxLength * kBoxLength;
    const float width2 = kBoxWidth * kBoxWidth;
    const float height2 = kBoxHeight * kBoxHeight;
    return {.x = mass / kBoxLength * (width2 + height2) / 12.0F,
            .y = mass / kBoxWidth * (length2 + height2) / 12.0F,
            .z = mass / kBoxHeight * (length2 + width2) / 12.0F};
}

std::array<Vector3, kBodyPointCount> BodyPoints(const ChassisSpec& chassis) {
    const float x = chassis.shadowLength / 2.0F;
    const float y = chassis.shadowWidth / 2.0F;
    return {{{.x = x, .y = y, .z = kBodyPointHeight},
             {.x = x, .y = -y, .z = kBodyPointHeight},
             {.x = -x, .y = y, .z = kBodyPointHeight},
             {.x = -x, .y = -y, .z = kBodyPointHeight}}};
}

int WeightBonus(const CarBuild& build) {
    return static_cast<int>(static_cast<float>(build.highPieceCount) * kStatPerPiece);
}

float SpeedMultiplier(int stat) {
    return 1.0F - (static_cast<float>(50 - stat) * 0.001F);
}

}

BodyConstants MakeBodyConstants(const ChassisSpec& chassis, const CarBuild& build) {
    const float mass = chassis.baseMass + static_cast<float>(build.highPieceCount);
    const std::array<Vector3, kWheelCount>& wheels = chassis.wheelPositions;
    return {
        .mass = mass,
        .massScale = mass * 0.001F * 0.001F,
        .inertia = BoxInertia(mass),
        .centreOfMass =
            chassis.centreOfMass + Vector3{.x = build.centroidOffsetX, .y = build.centroidOffsetY},
        .wheelAnchor = wheels[1],
        .trackWidth = wheels[3].y - wheels[2].y,
        .wheelbase = wheels[1].x - wheels[3].x,
        .bodyPoints = BodyPoints(chassis),
        .boundsRadius = build.boundsRadius,
    };
}

StatMultipliers MakeStatMultipliers(const ChassisSpec& chassis, const CarBuild& build) {
    const int bonus = WeightBonus(build);
    const int handling = std::min(100, chassis.handling + bonus);
    const int topSpeed = std::min(100, chassis.topSpeed + bonus);
    const int acceleration = std::max(0, chassis.acceleration - bonus);
    return {.handling = 0.7F + (0.003F * static_cast<float>(handling)),
            .acceleration = SpeedMultiplier(acceleration),
            .topSpeed = SpeedMultiplier(topSpeed)};
}

}
