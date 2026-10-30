#include "sim/physics/prebuilt_chassis.hpp"

namespace rr::sim {

ChassisSpec PrebuiltChassis(const assets::ChampionEntry& champion,
                            const assets::ChassisEntry& chassis) {
    return {.baseMass = champion.mass,
            .centreOfMass = champion.centreOfMass,
            .shadowWidth = chassis.shadowWidth,
            .shadowLength = chassis.shadowLength,
            .wheelPositions = chassis.wheelPositions,
            .handling = chassis.handling,
            .topSpeed = chassis.topSpeed,
            .acceleration = chassis.acceleration};
}

}
