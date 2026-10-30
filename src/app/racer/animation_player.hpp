#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "assets/animation_database.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"

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

    /// Writes the animated nodes into `locals`. Nodes without keys keep their values.
    void PoseNodes(std::span<Transform> locals);

private:
    struct Clock {
        std::size_t part = 0;
        float frame = 0.0F;
        bool loops = true;
    };

    AnimationPlayer(const assets::AnimationAsset& animations, std::size_t nodeCount)
        : animations_(&animations), blended_(nodeCount) {}

    void Wrap(Clock& clock) const;
    void Sample(const Clock& clock, std::span<Transform> locals) const;

    const assets::AnimationAsset* animations_;
    Clock current_;
    std::optional<Clock> next_;
    float blendMs_ = 0.0F;
    float blendedMs_ = 0.0F;
    std::vector<Transform> blended_;
};

}
