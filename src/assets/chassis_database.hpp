#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/model_files.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

inline constexpr std::size_t kChassisWheelCount = 4;

/// File names without their extensions.
struct JointedModelFiles {
    ModelFiles model;
    std::string skeleton;
    std::string animations;

    friend bool operator==(const JointedModelFiles&, const JointedModelFiles&) = default;
};

struct SkidMarks {
    float frontWidth = 0.0F;
    float rearWidth = 0.0F;
    std::array<Vector3, kChassisWheelCount> wheelOffsets{};
};

/// Positions are in world units in the car's space, +X forward and +Z up.
struct ChassisEntry {
    std::string name;
    JointedModelFiles wheels;
    std::optional<JointedModelFiles> bodyAttachment;
    Vector3 builtCentreOfMass;
    Vector3 seat;
    float baseMass = 0.0F;
    float shadowWidth = 0.0F;
    float shadowLength = 0.0F;
    std::string shadowTexture;
    float enginePitchScale = 0.0F;
    SkidMarks skids;
    std::array<Vector3, kChassisWheelCount> wheelPositions{};
    std::uint8_t handling = 50;
    std::uint8_t topSpeed = 50;
    std::uint8_t acceleration = 80;
};

[[nodiscard]] Result<std::vector<ChassisEntry>> ParseChassis(std::span<const std::byte> bytes);

}
