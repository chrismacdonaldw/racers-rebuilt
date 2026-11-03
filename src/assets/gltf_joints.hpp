#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"

#include <cgltf.h>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>

namespace rr::assets {

/// Takes glTF positions into the space of `joint`, still in glTF axes.
struct JointSpace {
    glm::mat4 position{1.0F};
    glm::mat3 normal{1.0F};
    std::uint16_t joint = 0;
};

/// Matches glTF nodes to at most 65,536 skeleton joint names, ignoring case.
class GltfJoints {
public:
    GltfJoints(const cgltf_data& data, std::span<const std::string> names);

    /// The nearest joint at or above `node`, else joint 0. Without joint names, world space.
    [[nodiscard]] JointSpace ForNode(const cgltf_node& node) const;

    /// One space per skin joint, through its inverse bind matrix. Fails on an unnamed joint.
    [[nodiscard]] Result<std::vector<JointSpace>> ForSkin(const cgltf_skin& skin) const;

    [[nodiscard]] bool HasJoints() const { return !nodes_.empty(); }

private:
    [[nodiscard]] std::optional<std::size_t> IndexOf(const cgltf_node& node) const;

    std::vector<const cgltf_node*> nodes_;
};

}
