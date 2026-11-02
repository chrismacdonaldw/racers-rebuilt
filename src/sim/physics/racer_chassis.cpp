#include "sim/physics/racer_chassis.hpp"

namespace rr::sim {
namespace {

constexpr float kCentroidShift = 0.25F;

}

ChassisSpec PrebuiltChassis(const assets::ChampionEntry& champion,
                            const assets::ChassisEntry& chassis) {
    ChassisSpec spec = BuiltChassis(chassis);
    spec.baseMass = champion.mass;
    spec.centreOfMass = champion.centreOfMass;
    return spec;
}

ChassisSpec BuiltChassis(const assets::ChassisEntry& chassis) {
    return {.baseMass = chassis.baseMass,
            .centreOfMass = chassis.builtCentreOfMass,
            .shadowWidth = chassis.shadowWidth,
            .shadowLength = chassis.shadowLength,
            .wheelPositions = chassis.wheelPositions,
            .handling = chassis.handling,
            .topSpeed = chassis.topSpeed,
            .acceleration = chassis.acceleration};
}

CarBuild BuiltCarBuild(const BrickVolume& bricks) {
    return {.highPieceCount = bricks.cells,
            .centroidOffsetX = bricks.centroid.x * kCentroidShift,
            .centroidOffsetY = bricks.centroid.y * kCentroidShift};
}

}
