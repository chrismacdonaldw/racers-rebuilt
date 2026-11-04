#pragma once

#include <span>
#include <vector>

#include "assets/skeleton_database.hpp"
#include "core/transform.hpp"

namespace rr::sim {

class FigureRetargeter {
public:
    /// `models` are the skeleton's model-space poses at `scale`. Rig limbs reach for the limb ends.
    void Run(const assets::SkeletonAsset& skeleton, float scale, std::span<Transform> models);

private:
    std::vector<Transform> posed_;
};

/// `rig` with `body`'s rest models as the pose its figure was fitted in.
[[nodiscard]] assets::FigureRig BoundRig(const assets::FigureRig& rig,
                                         const assets::SkeletonAsset& body);

}
