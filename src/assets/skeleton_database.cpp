#include "assets/skeleton_database.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

#include "assets/stored_rotation.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kNodeField = 0x27;

struct StoredNode {
    std::string name;
    Vector3 position;
    StoredRotation rotation = kStoredIdentity;
    std::string parent;
};

constexpr std::array kNodeFields{
    Field<&StoredNode::position>(0x28),
    Field<&StoredNode::rotation>(0x29),
    Field<&StoredNode::parent>(0x2A),
};

Result<std::optional<std::size_t>> FindParent(std::span<const SkeletonNode> earlier,
                                              const StoredNode& node) {
    if (node.parent.empty()) {
        return std::nullopt;
    }
    const auto parent = std::ranges::find(earlier, node.parent, &SkeletonNode::name);
    if (parent == earlier.end()) {
        return Fail(ErrorCode::InvalidData, std::format("node {} names parent {} before it appears",
                                                        node.name, node.parent));
    }
    return static_cast<std::size_t>(parent - earlier.begin());
}

Result<SkeletonNode> Decode(std::span<const SkeletonNode> earlier, const StoredNode& stored) {
    Result<std::optional<std::size_t>> parent = FindParent(earlier, stored);
    if (!parent) {
        return std::unexpected(std::move(parent.error()));
    }
    return FromStoredRotation(stored.rotation).transform([&](const Quaternion& rotation) {
        return SkeletonNode{.name = stored.name,
                            .position = stored.position,
                            .rotation = rotation,
                            .parent = *parent};
    });
}

}

Result<SkeletonAsset> ParseSkeleton(std::span<const std::byte> bytes) {
    Result<std::vector<StoredNode>> stored =
        ParseNamedLibrary<StoredNode>(bytes, kNodeField, kNodeFields);
    if (!stored) {
        return std::unexpected(std::move(stored.error()));
    }
    if (stored->empty()) {
        return Fail(ErrorCode::InvalidData, "skeleton has no nodes");
    }
    SkeletonAsset skeleton;
    skeleton.nodes.reserve(stored->size());
    for (const StoredNode& node : *stored) {
        Result<SkeletonNode> decoded = Decode(skeleton.nodes, node);
        if (!decoded) {
            return std::unexpected(std::move(decoded.error()));
        }
        skeleton.nodes.push_back(std::move(*decoded));
    }
    return skeleton;
}

}
