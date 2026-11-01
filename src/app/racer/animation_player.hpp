#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "assets/animation_database.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "sim/animation/part_clock.hpp"

namespace rr::app {

/// Plays the parts of one skeleton's animations. `animations` must outlive the player.
class AnimationPlayer {
public:
    /// Fails when a part has fewer tracks than the skeleton has nodes.
    [[nodiscard]] static Result<AnimationPlayer> Create(const assets::AnimationAsset& animations,
                                                        std::size_t nodeCount);

    [[nodiscard]] std::optional<std::size_t> FindPart(std::string_view name) const;

    /// Starts `part` at once, unless it is already playing.
    void Play(std::size_t part, bool loops);

    /// Loops `part`, blending to it from the current part.
    void BlendTo(std::size_t part, float blendMs);

    /// Runs each part's clock at its own frame rate.
    void Advance(float elapsedMs);

    void AdvanceFrames(float frames);

    /// Whether a part started with `loops` false is still short of its last frame.
    [[nodiscard]] bool IsPlayingOnce() const;

    /// Writes the animated nodes into `locals`. Nodes without keys keep their values.
    void PoseNodes(std::span<Transform> locals);

private:
    AnimationPlayer(const assets::AnimationAsset& animations, std::size_t nodeCount)
        : animations_(&animations), blended_(nodeCount) {}

    const assets::AnimationAsset* animations_;
    sim::PartClock current_;
    std::optional<sim::PartClock> next_;
    float blendMs_ = 0.0F;
    float blendedMs_ = 0.0F;
    std::vector<Transform> blended_;
};

}
