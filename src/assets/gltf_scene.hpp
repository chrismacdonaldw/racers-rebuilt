#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/geometry_asset.hpp"
#include "assets/glb_writer.hpp"
#include "core/quaternion.hpp"
#include "core/vector3.hpp"

#include <glm/mat4x4.hpp>

namespace rr::assets {

/// Placed in game space, relative to its parent. `extras` is a JSON object, or empty for none.
struct GltfNode {
    std::string name;
    Vector3 position;
    Quaternion rotation;
    float scale = 1.0F;
    std::optional<std::size_t> mesh;
    std::vector<std::size_t> children;
    std::string extras;
};

/// A static scene of meshes, lines and empties, written as one GLB.
class GltfScene {
public:
    /// Unskinned, posed by `binds` when given, sharing materials by name with earlier meshes.
    [[nodiscard]] std::size_t AddMesh(const GeometryAsset& geometry,
                                      std::span<const glm::mat4> binds = {});

    /// A line strip through `points`, in the space of the node that holds it.
    [[nodiscard]] std::size_t AddLine(std::span<const Vector3> points);

    std::size_t AddNode(GltfNode node);

    [[nodiscard]] GltfNode& Node(std::size_t index) { return nodes_.at(index); }

    [[nodiscard]] std::vector<std::byte> Build(std::span<const std::size_t> roots) const;

private:
    std::size_t MaterialOf(const std::string& name);

    GlbWriter writer_;
    std::vector<std::string> meshes_;
    std::vector<std::string> materials_;
    std::vector<GltfNode> nodes_;
};

}
