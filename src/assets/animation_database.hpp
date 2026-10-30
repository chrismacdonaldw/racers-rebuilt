#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "core/quaternion.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

/// `count` values from `firstValue` in a key pool, at the frames from `firstFrame` in
/// `AnimationAsset::keyFrames`. A single key has no frame.
struct KeyRun {
    std::uint32_t firstValue = 0;
    std::uint32_t firstFrame = 0;
    std::uint32_t count = 0;

    friend constexpr bool operator==(const KeyRun&, const KeyRun&) = default;
};

struct AnimationTrack {
    KeyRun rotations;
    KeyRun positions;

    friend constexpr bool operator==(const AnimationTrack&, const AnimationTrack&) = default;
};

struct BoundingSphere {
    Vector3 centre;
    float radius = 0.0F;
};

/// Node `i` of the skeleton plays track `firstTrack + i`.
struct AnimationPart {
    std::string name;
    std::uint32_t firstTrack = 0;
    std::int32_t frameCount = 0;
    std::int32_t loopFrames = 0;
    std::int32_t framesPerSecond = 0;
    Vector3 velocity;
    BoundingSphere bounds;
};

/// Positions and rotations are in the skeleton's conventions. Every key run is within its pools.
struct AnimationAsset {
    std::vector<Vector3> positionKeys;
    std::vector<Quaternion> rotationKeys;
    std::vector<std::uint16_t> keyFrames;
    std::vector<AnimationTrack> tracks;
    std::vector<AnimationPart> parts;
};

[[nodiscard]] Result<AnimationAsset> ParseAnimations(std::span<const std::byte> bytes);

}
