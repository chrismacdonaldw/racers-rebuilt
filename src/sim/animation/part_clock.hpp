#pragma once

#include <cstddef>
#include <span>

#include "assets/animation_database.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"

namespace rr::sim {

struct PartClock {
    std::size_t part = 0;
    float frame = 0.0F;
    bool loops = true;

    friend bool operator==(const PartClock&, const PartClock&) = default;
};

/// Fails when a part has fewer tracks than the skeleton has nodes.
[[nodiscard]] Result<void> CheckPartTracks(const assets::AnimationAsset& animations,
                                           std::size_t nodeCount);

/// Past the last frame, a looping part wraps over its loop length and a one-shot part holds.
void AdvanceFrames(const assets::AnimationAsset& animations, PartClock& clock, float frames);

void AdvanceClock(const assets::AnimationAsset& animations, PartClock& clock, float elapsedMs);

[[nodiscard]] bool IsHoldingLastFrame(const assets::AnimationAsset& animations,
                                      const PartClock& clock);

/// Writes the keyed nodes into `locals`. Nodes without keys keep their values.
void SamplePart(const assets::AnimationAsset& animations, const PartClock& clock,
                std::span<Transform> locals);

/// Blends towards `next` within one part. A wrap or a part change jumps to `next`.
[[nodiscard]] PartClock InterpolateClock(const PartClock& previous, const PartClock& next,
                                         float alpha);

}
