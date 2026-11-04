#include "assets/gltf_export.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "assets/glb_writer.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace rr::assets {
namespace {

constexpr float kChannelMax = 255.0F;
constexpr std::size_t kMatrixFloats = 16;

glm::vec3 ToGltf(const Vector3& game) {
    return {game.x, game.z, -game.y};
}

glm::quat ToGltf(const Quaternion& game) {
    const glm::vec3 axis = ToGltf(Vector3{.x = game.x, .y = game.y, .z = game.z});
    return {game.w, axis.x, axis.y, axis.z};
}

std::string Separator(const std::string& list) {
    return list.empty() ? "" : ",";
}

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

struct VertexData {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> colors;
    std::vector<float> uvs;
    std::vector<std::uint16_t> joints;
    std::vector<float> weights;
};

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

struct Channels {
    std::string channels;
    std::string samplers;
    std::size_t count = 0;
};

class GltfExporter {
public:
    explicit GltfExporter(const GltfSource& source)
        : source_(source),
          geometry_(*source.geometry),
          nodeCount_(source.skeleton == nullptr ? 0 : source.skeleton->nodes.size()) {
        if (source.skeleton != nullptr) {
            binds_ = BindPose(*source.skeleton, geometry_.scale);
        }
    }

    Result<std::vector<std::byte>> Export() {
        Result<std::string> mesh = Mesh();
        if (!mesh) {
            return std::unexpected(std::move(mesh.error()));
        }
        std::string fields = std::format(
            R"("scene":0,"scenes":[{{"nodes":[{}]}}],"nodes":[{}],"meshes":[{}],"materials":[{}])",
            Roots(), Nodes(), *mesh, Materials());
        if (nodeCount_ > 0) {
            fields += std::format(R"(,"skins":[{{"joints":[{}],"inverseBindMatrices":{}}}])",
                                  Joints(), InverseBinds());
        }
        Result<std::string> animations = Animations();
        if (!animations) {
            return std::unexpected(std::move(animations.error()));
        }
        if (!animations->empty()) {
            fields += std::format(R"(,"animations":[{}])", *animations);
        }
        return writer_.Build(fields);
    }

private:
    [[nodiscard]] Result<VertexData> Vertices() const {
        VertexData data;
        const bool isColored = IsColored(geometry_);
        for (const GeometryVertex& vertex : geometry_.vertices) {
            if (nodeCount_ > 0 && vertex.joint >= nodeCount_) {
                return Fail(ErrorCode::InvalidData,
                            std::format("a vertex uses joint {} of a {}-node skeleton",
                                        vertex.joint, nodeCount_));
            }
            const glm::mat4 bind = nodeCount_ > 0 ? binds_[vertex.joint] : glm::mat4{1.0F};
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
            data.joints.insert(data.joints.end(), {vertex.joint, 0, 0, 0});
            data.weights.insert(data.weights.end(), {1.0F, 0.0F, 0.0F, 0.0F});
        }
        return data;
    }

    Result<std::string> Mesh() {
        Result<VertexData> data = Vertices();
        if (!data) {
            return std::unexpected(std::move(data.error()));
        }
        std::string primitives;
        for (std::uint32_t material = 0; material < geometry_.materials.size(); ++material) {
            const Primitive primitive = PrimitiveOf(geometry_, material);
            if (!primitive.indices.empty()) {
                primitives +=
                    std::format(R"({}{{"attributes":{{{}}},"indices":{},"material":{}}})",
                                Separator(primitives), Attributes(Pick(*data, primitive.vertices)),
                                writer_.AddIndices(primitive.indices), material);
            }
        }
        return std::format(R"({{"primitives":[{}]}})", primitives);
    }

    std::string Attributes(const VertexData& data) {
        std::string attributes = std::format(R"("POSITION":{},"TEXCOORD_0":{})",
                                             writer_.AddFloats(data.positions, "VEC3", true),
                                             writer_.AddFloats(data.uvs, "VEC2"));
        attributes += data.normals.empty()
                          ? std::format(R"(,"COLOR_0":{})", writer_.AddFloats(data.colors, "VEC4"))
                          : std::format(R"(,"NORMAL":{})", writer_.AddFloats(data.normals, "VEC3"));
        if (nodeCount_ > 0) {
            attributes +=
                std::format(R"(,"JOINTS_0":{},"WEIGHTS_0":{})", writer_.AddJoints(data.joints),
                            writer_.AddFloats(data.weights, "VEC4"));
        }
        return attributes;
    }

    [[nodiscard]] std::string Materials() const {
        std::string materials;
        for (const std::string& name : geometry_.materials) {
            materials += std::format(R"({}{{"name":{},"doubleSided":true}})", Separator(materials),
                                     JsonString(name));
        }
        return materials;
    }

    [[nodiscard]] std::string Roots() const {
        std::string roots;
        for (std::size_t i = 0; i < nodeCount_; ++i) {
            if (!source_.skeleton->nodes[i].parent) {
                roots += std::format("{},", i);
            }
        }
        return roots + std::to_string(nodeCount_);
    }

    [[nodiscard]] std::string Children(std::size_t parent) const {
        std::string children;
        for (std::size_t i = 0; i < nodeCount_; ++i) {
            if (source_.skeleton->nodes[i].parent == parent) {
                children += std::format("{}{}", Separator(children), i);
            }
        }
        return children.empty() ? children : std::format(R"(,"children":[{}])", children);
    }

    [[nodiscard]] std::string Nodes() const {
        std::string nodes;
        for (std::size_t i = 0; i < nodeCount_; ++i) {
            const SkeletonNode& node = source_.skeleton->nodes[i];
            const glm::vec3 translation = ToGltf(node.position * geometry_.scale);
            const glm::quat rotation = ToGltf(node.rotation);
            nodes +=
                std::format(R"({{"name":{},"translation":[{},{},{}],"rotation":[{},{},{},{}]{}}},)",
                            JsonString(node.name), translation.x, translation.y, translation.z,
                            rotation.x, rotation.y, rotation.z, rotation.w, Children(i));
        }
        return nodes + (nodeCount_ > 0 ? R"({"mesh":0,"skin":0})" : R"({"mesh":0})");
    }

    [[nodiscard]] std::string Joints() const {
        std::string joints;
        for (std::size_t i = 0; i < nodeCount_; ++i) {
            joints += std::format("{}{}", Separator(joints), i);
        }
        return joints;
    }

    std::size_t InverseBinds() {
        std::vector<float> matrices;
        matrices.reserve(binds_.size() * kMatrixFloats);
        for (const glm::mat4& bind : binds_) {
            const glm::mat4 inverse = glm::inverse(bind);
            const std::span<const float> values(glm::value_ptr(inverse), kMatrixFloats);
            matrices.insert(matrices.end(), values.begin(), values.end());
        }
        return writer_.AddFloats(matrices, "MAT4");
    }

    Result<std::string> Animations() {
        std::string animations;
        if (source_.animations == nullptr || nodeCount_ == 0) {
            return animations;
        }
        for (const AnimationPart& part : source_.animations->parts) {
            if (part.firstTrack + nodeCount_ > source_.animations->tracks.size()) {
                return Fail(ErrorCode::InvalidData,
                            std::format("the {} animation has fewer tracks than the skeleton has "
                                        "nodes",
                                        part.name));
            }
            const std::string animation = Animation(part);
            if (!animation.empty()) {
                animations += Separator(animations) + animation;
            }
        }
        return animations;
    }

    std::string Animation(const AnimationPart& part) {
        if (part.framesPerSecond <= 0) {
            return {};
        }
        Channels channels;
        for (std::size_t node = 0; node < nodeCount_; ++node) {
            const AnimationTrack& track = source_.animations->tracks[part.firstTrack + node];
            AddChannel(channels, node, "rotation", track.rotations, part);
            AddChannel(channels, node, "translation", track.positions, part);
        }
        if (channels.count == 0) {
            return {};
        }
        return std::format(R"({{"name":{},"channels":[{}],"samplers":[{}]}})",
                           JsonString(part.name), channels.channels, channels.samplers);
    }

    void AddChannel(Channels& out, std::size_t node, std::string_view path, const KeyRun& run,
                    const AnimationPart& part) {
        if (run.count == 0) {
            return;
        }
        const bool isRotation = path == "rotation";
        const std::size_t times = writer_.AddFloats(Times(run, part), "SCALAR", true);
        const std::size_t values = isRotation ? writer_.AddFloats(Rotations(run), "VEC4")
                                              : writer_.AddFloats(Translations(run), "VEC3");
        out.channels += std::format(R"({}{{"sampler":{},"target":{{"node":{},"path":"{}"}}}})",
                                    Separator(out.channels), out.count, node, path);
        out.samplers += std::format(R"({}{{"input":{},"output":{},"interpolation":"LINEAR"}})",
                                    Separator(out.samplers), times, values);
        ++out.count;
    }

    [[nodiscard]] std::vector<float> Times(const KeyRun& run, const AnimationPart& part) const {
        std::vector<float> times;
        const auto rate = static_cast<float>(part.framesPerSecond);
        for (std::uint32_t i = 0; i < run.count; ++i) {
            const std::uint16_t frame =
                run.count > 1 ? source_.animations->keyFrames[run.firstFrame + i] : 0;
            times.push_back(static_cast<float>(frame) / rate);
        }
        return times;
    }

    [[nodiscard]] std::vector<float> Rotations(const KeyRun& run) const {
        std::vector<float> values;
        for (std::uint32_t i = 0; i < run.count; ++i) {
            const glm::quat key =
                glm::normalize(ToGltf(source_.animations->rotationKeys[run.firstValue + i]));
            values.insert(values.end(), {key.x, key.y, key.z, key.w});
        }
        return values;
    }

    [[nodiscard]] std::vector<float> Translations(const KeyRun& run) const {
        std::vector<float> values;
        for (std::uint32_t i = 0; i < run.count; ++i) {
            const glm::vec3 key =
                ToGltf(source_.animations->positionKeys[run.firstValue + i] * geometry_.scale);
            values.insert(values.end(), {key.x, key.y, key.z});
        }
        return values;
    }

    const GltfSource& source_;
    const GeometryAsset& geometry_;
    std::size_t nodeCount_;
    std::vector<glm::mat4> binds_;
    GlbWriter writer_;
};

}

Result<std::vector<std::byte>> ExportGltf(const GltfSource& source) {
    if (source.geometry == nullptr || source.geometry->vertices.empty()) {
        return Fail(ErrorCode::InvalidData, "the model has no vertices");
    }
    return GltfExporter(source).Export();
}

}
