#include "assets/gltf_export.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "assets/glb_writer.hpp"
#include "assets/gltf_mesh.hpp"

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

constexpr std::size_t kMatrixFloats = 16;
constexpr float kKeyGap = 1.0e-4F;

std::string Separator(const std::string& list) {
    return list.empty() ? "" : ",";
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
    Result<std::string> Mesh() {
        for (const GeometryVertex& vertex : geometry_.vertices) {
            if (nodeCount_ > 0 && vertex.joint >= nodeCount_) {
                return Fail(ErrorCode::InvalidData,
                            std::format("a vertex uses joint {} of a {}-node skeleton",
                                        vertex.joint, nodeCount_));
            }
        }
        std::vector<std::size_t> materials(geometry_.materials.size());
        std::ranges::iota(materials, std::size_t{0});
        return WriteMesh(writer_, geometry_, {.binds = binds_, .isSkinned = nodeCount_ > 0},
                         materials);
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
            nodes += std::format(
                R"({{"name":{},"translation":[{},{},{}],"rotation":[{},{},{},{}]{}}},)",
                JsonString(node.name), JsonNumber(translation.x), JsonNumber(translation.y),
                JsonNumber(translation.z), JsonNumber(rotation.x), JsonNumber(rotation.y),
                JsonNumber(rotation.z), JsonNumber(rotation.w), Children(i));
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
            const float seconds = static_cast<float>(frame) / rate;
            times.push_back(times.empty() ? seconds : std::max(seconds, times.back() + kKeyGap));
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
