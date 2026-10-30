#pragma once

#include <span>
#include <vector>

#include "assets/skeleton_database.hpp"
#include "core/transform.hpp"

namespace rr::app {

/// `Solve` gives model-space node transforms, with positions multiplied by the model scale.
class SkeletonPose {
public:
    SkeletonPose(const assets::SkeletonAsset& skeleton, float scale);

    [[nodiscard]] std::span<Transform> Locals() { return locals_; }
    [[nodiscard]] std::span<const Transform> Locals() const { return locals_; }

    [[nodiscard]] std::span<const Transform> Solve();

private:
    std::vector<std::size_t> parents_;
    std::vector<Transform> locals_;
    std::vector<Transform> models_;
    float scale_;
};

}
