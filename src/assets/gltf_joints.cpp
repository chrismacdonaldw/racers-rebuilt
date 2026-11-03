#include "assets/gltf_joints.hpp"

#include <algorithm>
#include <array>
#include <format>

#include "assets/game_path.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace rr::assets {
namespace {

constexpr std::size_t kMatrixFloats = 16;

glm::mat4 WorldOf(const cgltf_node& node) {
    std::array<float, kMatrixFloats> world{};
    cgltf_node_transform_world(&node, world.data());
    return glm::make_mat4(world.data());
}

JointSpace SpaceOf(const glm::mat4& position, std::size_t joint) {
    return {.position = position,
            .normal = glm::inverseTranspose(glm::mat3(position)),
            .joint = static_cast<std::uint16_t>(joint)};
}

const cgltf_node* FindNode(const cgltf_data& data, const std::string& name) {
    const std::span nodes(data.nodes, data.nodes_count);
    const auto found = std::ranges::find_if(nodes, [&](const cgltf_node& node) {
        return node.name != nullptr && ToUpperAscii(node.name) == ToUpperAscii(name);
    });
    return found == nodes.end() ? nullptr : &*found;
}

glm::mat4 InverseBind(const cgltf_skin& skin, std::size_t joint) {
    glm::mat4 matrix{1.0F};
    if (skin.inverse_bind_matrices != nullptr) {
        cgltf_accessor_read_float(skin.inverse_bind_matrices, joint, glm::value_ptr(matrix),
                                  kMatrixFloats);
    }
    return matrix;
}

}

GltfJoints::GltfJoints(const cgltf_data& data, std::span<const std::string> names) {
    nodes_.reserve(names.size());
    for (const std::string& name : names) {
        nodes_.push_back(FindNode(data, name));
    }
}

JointSpace GltfJoints::ForNode(const cgltf_node& node) const {
    if (!HasJoints()) {
        return SpaceOf(WorldOf(node), 0);
    }
    std::size_t joint = 0;
    for (const cgltf_node* at = &node; at != nullptr; at = at->parent) {
        if (const std::optional<std::size_t> found = IndexOf(*at)) {
            joint = *found;
            break;
        }
    }
    const glm::mat4 toJoint =
        nodes_[joint] != nullptr ? glm::inverse(WorldOf(*nodes_[joint])) : glm::mat4{1.0F};
    return SpaceOf(toJoint * WorldOf(node), joint);
}

Result<std::vector<JointSpace>> GltfJoints::ForSkin(const cgltf_skin& skin) const {
    const cgltf_accessor* binds = skin.inverse_bind_matrices;
    if (binds != nullptr && (binds->type != cgltf_type_mat4 || binds->count < skin.joints_count ||
                             binds->buffer_view == nullptr || binds->is_sparse != 0)) {
        return Fail(ErrorCode::InvalidData, "a glTF skin's bind matrices can't be read");
    }
    std::vector<JointSpace> spaces;
    for (std::size_t i = 0; i < skin.joints_count; ++i) {
        const cgltf_node& node = *skin.joints[i];
        const std::optional<std::size_t> joint = IndexOf(node);
        if (!joint) {
            return Fail(ErrorCode::InvalidData,
                        std::format("the glTF joint {} isn't in the model's skeleton",
                                    node.name != nullptr ? node.name : "without a name"));
        }
        spaces.push_back(SpaceOf(InverseBind(skin, i), *joint));
    }
    return spaces;
}

std::optional<std::size_t> GltfJoints::IndexOf(const cgltf_node& node) const {
    const auto found = std::ranges::find(nodes_, &node);
    if (found == nodes_.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - nodes_.begin());
}

}
