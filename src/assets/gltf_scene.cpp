#include "assets/gltf_scene.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <utility>

#include "assets/gltf_mesh.hpp"

namespace rr::assets {
namespace {

constexpr int kLineStrip = 3;

std::string Joined(std::span<const std::string> items) {
    std::string joined;
    for (const std::string& item : items) {
        joined += (joined.empty() ? "" : ",") + item;
    }
    return joined;
}

std::string Indices(std::span<const std::size_t> indices) {
    std::string joined;
    for (const std::size_t index : indices) {
        joined += std::format("{}{}", joined.empty() ? "" : ",", index);
    }
    return joined;
}

std::string NodeJson(const GltfNode& node) {
    const glm::vec3 position = ToGltf(node.position);
    const glm::quat rotation = ToGltf(node.rotation);
    std::string json =
        std::format(R"({{"name":{},"translation":[{},{},{}],"rotation":[{},{},{},{}])",
                    JsonString(node.name), JsonNumber(position.x), JsonNumber(position.y),
                    JsonNumber(position.z), JsonNumber(rotation.x), JsonNumber(rotation.y),
                    JsonNumber(rotation.z), JsonNumber(rotation.w));
    if (node.scale != 1.0F) {
        const std::string scale = JsonNumber(node.scale);
        json += std::format(R"(,"scale":[{},{},{}])", scale, scale, scale);
    }
    if (node.mesh) {
        json += std::format(R"(,"mesh":{})", *node.mesh);
    }
    if (!node.children.empty()) {
        json += std::format(R"(,"children":[{}])", Indices(node.children));
    }
    if (!node.extras.empty()) {
        json += std::format(R"(,"extras":{})", node.extras);
    }
    return json + "}";
}

}

std::size_t GltfScene::AddMesh(const GeometryAsset& geometry, std::span<const glm::mat4> binds) {
    std::vector<std::size_t> materials;
    materials.reserve(geometry.materials.size());
    std::ranges::transform(geometry.materials, std::back_inserter(materials),
                           [&](const std::string& name) { return MaterialOf(name); });
    meshes_.push_back(WriteMesh(writer_, geometry, {.binds = binds}, materials));
    return meshes_.size() - 1;
}

std::size_t GltfScene::AddLine(std::span<const Vector3> points) {
    std::vector<float> positions;
    positions.reserve(points.size() * 3);
    for (const Vector3& point : points) {
        const glm::vec3 at = ToGltf(point);
        positions.insert(positions.end(), {at.x, at.y, at.z});
    }
    meshes_.push_back(
        std::format(R"({{"primitives":[{{"attributes":{{"POSITION":{}}},"mode":{}}}]}})",
                    writer_.AddFloats(positions, "VEC3", true), kLineStrip));
    return meshes_.size() - 1;
}

std::size_t GltfScene::AddNode(GltfNode node) {
    nodes_.push_back(std::move(node));
    return nodes_.size() - 1;
}

std::vector<std::byte> GltfScene::Build(std::span<const std::size_t> roots) const {
    std::vector<std::string> nodes;
    nodes.reserve(nodes_.size());
    std::ranges::transform(nodes_, std::back_inserter(nodes), NodeJson);
    std::vector<std::string> materials;
    materials.reserve(materials_.size());
    std::ranges::transform(materials_, std::back_inserter(materials), [](const std::string& name) {
        return std::format(R"({{"name":{},"doubleSided":true}})", JsonString(name));
    });
    std::string fields = std::format(R"("scene":0,"scenes":[{{"nodes":[{}]}}],"nodes":[{}])",
                                     Indices(roots), Joined(nodes));
    if (!meshes_.empty()) {
        fields += std::format(R"(,"meshes":[{}])", Joined(meshes_));
    }
    if (!materials.empty()) {
        fields += std::format(R"(,"materials":[{}])", Joined(materials));
    }
    return writer_.Build(fields);
}

std::size_t GltfScene::MaterialOf(const std::string& name) {
    const auto found = std::ranges::find(materials_, name);
    if (found != materials_.end()) {
        return static_cast<std::size_t>(found - materials_.begin());
    }
    materials_.push_back(name);
    return materials_.size() - 1;
}

}
