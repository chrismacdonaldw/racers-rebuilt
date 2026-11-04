#include "assets/gltf_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <vector>

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec4.hpp>

namespace rr::assets {
namespace {

constexpr float kChannelMax = 255.0F;

bool IsColored(const GeometryAsset& geometry) {
    return std::ranges::all_of(
        geometry.vertices, [](const GeometryVertex& vertex) { return vertex.normal == Vector3{}; });
}

float ToLinear(std::uint8_t channel) {
    constexpr float kLinearCutoff = 0.04045F;
    constexpr float kLinearSlope = 12.92F;
    constexpr float kGammaScale = 1.055F;
    constexpr float kGammaOffset = 0.055F;
    constexpr float kGamma = 2.4F;
    const float srgb = static_cast<float>(channel) / kChannelMax;
    return srgb <= kLinearCutoff ? srgb / kLinearSlope
                                 : std::pow((srgb + kGammaOffset) / kGammaScale, kGamma);
}

glm::vec3 Normalized(const glm::vec3& normal) {
    return glm::dot(normal, normal) > 0.0F ? glm::normalize(normal) : glm::vec3{0.0F, 1.0F, 0.0F};
}

struct VertexData {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> colors;
    std::vector<float> uvs;
    std::vector<std::uint16_t> joints;
    std::vector<float> weights;
};

VertexData VerticesOf(const GeometryAsset& geometry, const MeshPose& pose) {
    const std::span<const glm::mat4> binds = pose.binds;
    VertexData data;
    const bool isColored = IsColored(geometry);
    for (const GeometryVertex& vertex : geometry.vertices) {
        const glm::mat4 bind = vertex.joint < binds.size() ? binds[vertex.joint] : glm::mat4{1.0F};
        const glm::vec3 position(bind * glm::vec4(ToGltf(vertex.position), 1.0F));
        data.positions.insert(data.positions.end(), {position.x, position.y, position.z});
        if (isColored) {
            const Rgba8& rgba = vertex.rgba;
            data.colors.insert(data.colors.end(),
                               {ToLinear(rgba[0]), ToLinear(rgba[1]), ToLinear(rgba[2]),
                                static_cast<float>(rgba[3]) / kChannelMax});
        } else {
            const glm::vec3 normal = Normalized(glm::mat3(bind) * ToGltf(vertex.normal));
            data.normals.insert(data.normals.end(), {normal.x, normal.y, normal.z});
        }
        data.uvs.insert(data.uvs.end(), {vertex.u, vertex.v});
        if (pose.isSkinned) {
            data.joints.insert(data.joints.end(), {vertex.joint, 0, 0, 0});
            data.weights.insert(data.weights.end(), {1.0F, 0.0F, 0.0F, 0.0F});
        }
    }
    return data;
}

template <typename Value>
std::vector<Value> Pick(const std::vector<Value>& values, std::size_t stride,
                        std::span<const std::uint32_t> vertices) {
    std::vector<Value> picked;
    if (values.empty()) {
        return picked;
    }
    picked.reserve(vertices.size() * stride);
    for (const std::uint32_t vertex : vertices) {
        const auto first = values.begin() + static_cast<std::ptrdiff_t>(vertex * stride);
        picked.insert(picked.end(), first, first + static_cast<std::ptrdiff_t>(stride));
    }
    return picked;
}

VertexData Pick(const VertexData& data, std::span<const std::uint32_t> vertices) {
    constexpr std::size_t kVec2 = 2;
    constexpr std::size_t kVec3 = 3;
    constexpr std::size_t kVec4 = 4;
    return {.positions = Pick(data.positions, kVec3, vertices),
            .normals = Pick(data.normals, kVec3, vertices),
            .colors = Pick(data.colors, kVec4, vertices),
            .uvs = Pick(data.uvs, kVec2, vertices),
            .joints = Pick(data.joints, kVec4, vertices),
            .weights = Pick(data.weights, kVec4, vertices)};
}

struct Primitive {
    std::vector<std::uint32_t> vertices;
    std::vector<std::uint32_t> indices;
};

Primitive PrimitiveOf(const GeometryAsset& geometry, std::uint32_t material) {
    Primitive primitive;
    std::vector<std::optional<std::uint32_t>> local(geometry.vertices.size());
    for (const GeometryRange& range : geometry.ranges) {
        if (range.material != material) {
            continue;
        }
        for (std::uint32_t i = 0; i < range.indexCount; ++i) {
            const std::uint32_t vertex = geometry.indices[range.firstIndex + i];
            if (!local[vertex]) {
                local[vertex] = static_cast<std::uint32_t>(primitive.vertices.size());
                primitive.vertices.push_back(vertex);
            }
            primitive.indices.push_back(*local[vertex]);
        }
    }
    return primitive;
}

std::string Attributes(GlbWriter& writer, const VertexData& data) {
    std::string attributes = std::format(R"("POSITION":{},"TEXCOORD_0":{})",
                                         writer.AddFloats(data.positions, "VEC3", true),
                                         writer.AddFloats(data.uvs, "VEC2"));
    attributes += data.normals.empty()
                      ? std::format(R"(,"COLOR_0":{})", writer.AddFloats(data.colors, "VEC4"))
                      : std::format(R"(,"NORMAL":{})", writer.AddFloats(data.normals, "VEC3"));
    if (!data.joints.empty()) {
        attributes += std::format(R"(,"JOINTS_0":{},"WEIGHTS_0":{})", writer.AddJoints(data.joints),
                                  writer.AddFloats(data.weights, "VEC4"));
    }
    return attributes;
}

}

glm::vec3 ToGltf(const Vector3& game) {
    return {game.x, game.z, -game.y};
}

glm::quat ToGltf(const Quaternion& game) {
    const glm::vec3 axis = ToGltf(Vector3{.x = game.x, .y = game.y, .z = game.z});
    return {game.w, axis.x, axis.y, axis.z};
}

std::vector<glm::mat4> BindPose(const SkeletonAsset& skeleton, float scale) {
    std::vector<glm::mat4> binds;
    binds.reserve(skeleton.nodes.size());
    for (const SkeletonNode& node : skeleton.nodes) {
        const glm::mat4 local = glm::translate(glm::mat4{1.0F}, ToGltf(node.position * scale)) *
                                glm::mat4_cast(ToGltf(node.rotation));
        binds.push_back(node.parent ? binds[*node.parent] * local : local);
    }
    return binds;
}

std::string WriteMesh(GlbWriter& writer, const GeometryAsset& geometry, const MeshPose& pose,
                      std::span<const std::size_t> materials) {
    const VertexData data = VerticesOf(geometry, pose);
    std::string primitives;
    for (std::uint32_t material = 0; material < materials.size(); ++material) {
        const Primitive primitive = PrimitiveOf(geometry, material);
        if (!primitive.indices.empty()) {
            primitives += std::format(R"({}{{"attributes":{{{}}},"indices":{},"material":{}}})",
                                      primitives.empty() ? "" : ",",
                                      Attributes(writer, Pick(data, primitive.vertices)),
                                      writer.AddIndices(primitive.indices), materials[material]);
        }
    }
    return std::format(R"({{"primitives":[{}]}})", primitives);
}

}
