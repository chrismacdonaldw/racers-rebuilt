#include "app/racer/posed_draws.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "app/track_space.hpp"

namespace rr::app {

Result<AnimatedSkeleton> AnimatedSkeleton::Bind(const JointedAssets& jointed,
                                                const ModelDraws& model) {
    const std::size_t nodeCount = jointed.skeleton.nodes.size();
    if (std::ranges::any_of(model.joints,
                            [&](std::uint16_t joint) { return joint >= nodeCount; })) {
        return Fail(ErrorCode::InvalidData,
                    std::format("a model follows a node past the skeleton's {}", nodeCount));
    }
    return AnimationPlayer::Create(jointed.animations, nodeCount)
        .transform([&](AnimationPlayer player) {
            return AnimatedSkeleton{.pose = sim::SkeletonPose(jointed.skeleton, model.scale),
                                    .player = std::move(player)};
        });
}

std::span<const Transform> AnimatedSkeleton::Solve() {
    player.PoseNodes(pose.Locals());
    return pose.Solve();
}

PosedDraws::Group PosedDraws::Append(const ModelDraws& model) {
    const Group group{.first = draws_.size(), .count = model.draws.size()};
    draws_.insert(draws_.end(), model.draws.begin(), model.draws.end());
    joints_.insert(joints_.end(), model.joints.begin(), model.joints.end());
    joints_.resize(draws_.size());
    return group;
}

void PosedDraws::Place(Group group, const Transform& entity, std::span<const Transform> nodes) {
    for (std::size_t i = group.first; i < group.first + group.count; ++i) {
        const std::uint16_t joint = joints_[i];
        const Transform placed = joint < nodes.size() ? Compose(entity, nodes[joint]) : entity;
        draws_[i].transform = TransformToEngine(placed);
    }
}

}
