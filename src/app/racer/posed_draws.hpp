#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "app/racer/animation_player.hpp"
#include "app/racer/racer_assets.hpp"
#include "app/racer/racer_draws.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "render/render_view.hpp"
#include "sim/animation/skeleton_pose.hpp"

namespace rr::app {

struct AnimatedSkeleton {
    sim::SkeletonPose pose;
    AnimationPlayer player;

    /// Fails on a node past the skeleton or a part short of tracks. `jointed` must outlive it.
    [[nodiscard]] static Result<AnimatedSkeleton> Bind(const JointedAssets& jointed,
                                                       const ModelDraws& model);

    /// Poses the nodes at the player's current frame, in model space.
    [[nodiscard]] std::span<const Transform> Solve();
};

/// The draws of several models, each group placed at its own entity and skinned by its nodes.
class PosedDraws {
public:
    struct Group {
        std::size_t first = 0;
        std::size_t count = 0;
        std::size_t poses = 0;
    };

    Group Append(const ModelDraws& model);

    /// `entity` is in game space, and `nodes` are its skeleton's model-space poses.
    void Place(Group group, const Transform& entity, std::span<const Transform> nodes = {});

    /// In engine space.
    [[nodiscard]] std::span<const render::MeshDraw> Draws() const { return draws_; }

private:
    std::vector<render::MeshDraw> draws_;
    std::vector<std::vector<Transform>> poses_;
};

}
