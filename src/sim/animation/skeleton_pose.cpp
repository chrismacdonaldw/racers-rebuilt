#include "sim/animation/skeleton_pose.hpp"

#include <cstddef>

namespace rr::sim {
namespace {

constexpr std::size_t kNoParent = static_cast<std::size_t>(-1);

}

SkeletonPose::SkeletonPose(const assets::SkeletonAsset& skeleton, float scale)
    : models_(skeleton.nodes.size()), scale_(scale) {
    parents_.reserve(skeleton.nodes.size());
    locals_.reserve(skeleton.nodes.size());
    for (const assets::SkeletonNode& node : skeleton.nodes) {
        parents_.push_back(node.parent.value_or(kNoParent));
        locals_.push_back({.position = node.position, .rotation = node.rotation});
    }
}

std::span<const Transform> SkeletonPose::Solve() {
    for (std::size_t i = 0; i < locals_.size(); ++i) {
        const Transform scaled{.position = locals_[i].position * scale_,
                               .rotation = locals_[i].rotation};
        models_[i] = parents_[i] == kNoParent ? scaled : Compose(models_[parents_[i]], scaled);
    }
    return models_;
}

}
