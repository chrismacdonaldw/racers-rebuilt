#include "assets/gltf_model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/gltf_data.hpp"
#include "assets/gltf_joints.hpp"

#include <cgltf.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace rr::assets {
namespace {

constexpr float kChannelMax = 255.0F;
constexpr float kLinearCutoff = 0.0031308F;
constexpr float kLinearSlope = 12.92F;
constexpr float kGammaScale = 1.055F;
constexpr float kGammaOffset = 0.055F;
constexpr float kInverseGamma = 1.0F / 2.4F;
constexpr std::size_t kMaxVertices = 4'000'000;
constexpr std::size_t kMaxIndices = 3 * kMaxVertices;
constexpr std::size_t kSkinInfluences = 4;
constexpr std::size_t kMaxJoints = std::size_t{std::numeric_limits<std::uint16_t>::max()} + 1;
constexpr Vector3 kUp{.x = 0.0F, .y = 0.0F, .z = 1.0F};

using SkinSpaces = std::vector<JointSpace>;

struct AccessorRule {
    const cgltf_accessor* accessor = nullptr;
    cgltf_type type = cgltf_type_invalid;
    cgltf_type alternative = cgltf_type_invalid;
};

struct Attributes {
    const cgltf_accessor* positions = nullptr;
    const cgltf_accessor* normals = nullptr;
    const cgltf_accessor* uvs = nullptr;
    const cgltf_accessor* colors = nullptr;
    const cgltf_accessor* joints = nullptr;
    const cgltf_accessor* weights = nullptr;
};

std::uint8_t ToSrgbChannel(float linear) {
    const float clamped = std::clamp(linear, 0.0F, 1.0F);
    const float srgb = clamped <= kLinearCutoff
                           ? clamped * kLinearSlope
                           : (kGammaScale * std::pow(clamped, kInverseGamma)) - kGammaOffset;
    return static_cast<std::uint8_t>(std::lround(srgb * kChannelMax));
}

std::uint8_t ToChannel(float value) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * kChannelMax));
}

template <typename Vector>
Vector Read(const cgltf_accessor* accessor, cgltf_size index) {
    Vector value{};
    value[Vector::length() - 1] = 1.0F;
    cgltf_accessor_read_float(accessor, index, glm::value_ptr(value),
                              static_cast<cgltf_size>(Vector::length()));
    return value;
}

Result<void> Expect(bool isTrue, std::string_view problem) {
    return isTrue ? Result<void>{} : Fail(ErrorCode::InvalidData, std::string(problem));
}

Result<void> CheckAccessor(const AccessorRule& rule) {
    const cgltf_accessor* accessor = rule.accessor;
    if (accessor == nullptr) {
        return {};
    }
    return Expect(accessor->buffer_view != nullptr && accessor->is_sparse == 0,
                  "compressed and sparse glTF accessors aren't supported")
        .and_then([&] {
            return Expect(accessor->type == rule.type || accessor->type == rule.alternative,
                          "a glTF mesh attribute has the wrong type");
        });
}

bool IsFinite(const Vector3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

Attributes FindAttributes(const cgltf_primitive& primitive) {
    return {.positions = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0),
            .normals = cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0),
            .uvs = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0),
            .colors = cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0),
            .joints = cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0),
            .weights = cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0)};
}

AccessorRule Rule(const cgltf_accessor* accessor, cgltf_type type) {
    return {.accessor = accessor, .type = type, .alternative = type};
}

Result<void> CheckPrimitive(const cgltf_primitive& primitive, const Attributes& attributes) {
    Result<void> checked =
        Expect(primitive.type == cgltf_primitive_type_triangles, "only triangles are supported")
            .and_then(
                [&] { return Expect(attributes.positions != nullptr, "a mesh has no positions"); })
            .and_then([&] {
                return Expect(attributes.normals != nullptr || attributes.colors != nullptr,
                              "a mesh needs normals or vertex colours");
            })
            .and_then([&] {
                return Expect(primitive.material != nullptr && primitive.material->name != nullptr,
                              "every mesh needs a named material");
            });
    const std::array rules{
        Rule(attributes.positions, cgltf_type_vec3),
        Rule(attributes.normals, cgltf_type_vec3),
        Rule(attributes.uvs, cgltf_type_vec2),
        AccessorRule{
            .accessor = attributes.colors, .type = cgltf_type_vec3, .alternative = cgltf_type_vec4},
        Rule(attributes.joints, cgltf_type_vec4),
        Rule(attributes.weights, cgltf_type_vec4),
        Rule(primitive.indices, cgltf_type_scalar)};
    for (const AccessorRule& rule : rules) {
        checked = checked.and_then([&rule] { return CheckAccessor(rule); });
    }
    return checked;
}

Vector3 ReadNormal(cgltf_size index, const JointSpace& space, const cgltf_accessor* normals) {
    const glm::vec3 normal = space.normal * Read<glm::vec3>(normals, index);
    return glm::dot(normal, normal) == 0.0F ? kUp : ToGame(glm::normalize(normal));
}

GeometryVertex ReadVertex(cgltf_size index, const JointSpace& space, const Attributes& attributes) {
    GeometryVertex vertex;
    vertex.position =
        ToGame(glm::vec3(space.position * Read<glm::vec4>(attributes.positions, index)));
    vertex.joint = space.joint;
    if (attributes.normals != nullptr) {
        vertex.normal = ReadNormal(index, space, attributes.normals);
    }
    if (attributes.uvs != nullptr) {
        const auto uv = Read<glm::vec2>(attributes.uvs, index);
        vertex.u = uv.x;
        vertex.v = uv.y;
    }
    if (attributes.colors != nullptr) {
        const auto color = Read<glm::vec4>(attributes.colors, index);
        vertex.rgba = {ToSrgbChannel(color.r), ToSrgbChannel(color.g), ToSrgbChannel(color.b),
                       ToChannel(color.a)};
    }
    return vertex;
}

Result<const JointSpace*> HeaviestJoint(cgltf_size index, const Attributes& attributes,
                                        const SkinSpaces& skin) {
    std::array<cgltf_uint, kSkinInfluences> joints{};
    cgltf_accessor_read_uint(attributes.joints, index, joints.data(), joints.size());
    const auto weights = Read<glm::vec4>(attributes.weights, index);
    std::size_t heaviest = 0;
    for (std::size_t i = 1; i < kSkinInfluences; ++i) {
        if (weights[static_cast<glm::length_t>(i)] >
            weights[static_cast<glm::length_t>(heaviest)]) {
            heaviest = i;
        }
    }
    if (joints.at(heaviest) >= skin.size()) {
        return Fail(ErrorCode::InvalidData, "a glTF vertex uses a joint past its skin");
    }
    return &skin[joints.at(heaviest)];
}

class Flattener {
public:
    Flattener(GltfModel& model, const GltfJoints& joints) : model_(model), joints_(joints) {}

    Result<void> AddScene(std::span<cgltf_node* const> roots) {
        std::vector<const cgltf_node*> pending(roots.rbegin(), roots.rend());
        while (!pending.empty()) {
            const cgltf_node* node = pending.back();
            pending.pop_back();
            if (Result<void> added = AddMesh(*node); !added) {
                return added;
            }
            const std::span children(node->children, node->children_count);
            pending.insert(pending.end(), children.rbegin(), children.rend());
        }
        return {};
    }

private:
    struct MeshSpaces {
        JointSpace rigid;
        std::optional<SkinSpaces> skin;
    };

    Result<void> AddMesh(const cgltf_node& node) {
        if (node.mesh == nullptr) {
            return {};
        }
        MeshSpaces spaces{.rigid = joints_.ForNode(node), .skin = std::nullopt};
        if (node.skin != nullptr && joints_.HasJoints()) {
            Result<SkinSpaces> skin = joints_.ForSkin(*node.skin);
            if (!skin) {
                return std::unexpected(std::move(skin.error()));
            }
            spaces.skin = std::move(*skin);
        }
        for (const cgltf_primitive& primitive :
             std::span(node.mesh->primitives, node.mesh->primitives_count)) {
            Result<void> added = AddPrimitive(primitive, spaces);
            if (!added) {
                return added;
            }
        }
        return {};
    }

    Result<void> AddPrimitive(const cgltf_primitive& primitive, const MeshSpaces& spaces) {
        const Attributes attributes = FindAttributes(primitive);
        Result<void> checked = CheckPrimitive(primitive, attributes);
        if (!checked) {
            return checked;
        }
        GeometryAsset& geometry = model_.geometry;
        const cgltf_size indexCount =
            primitive.indices != nullptr ? primitive.indices->count : attributes.positions->count;
        if (geometry.vertices.size() + attributes.positions->count > kMaxVertices ||
            geometry.indices.size() + indexCount > kMaxIndices) {
            return Fail(ErrorCode::InvalidData, "the glTF model has too many vertices");
        }
        const auto first = static_cast<std::uint32_t>(geometry.vertices.size());
        for (cgltf_size i = 0; i < attributes.positions->count; ++i) {
            Result<GeometryVertex> vertex = BuildVertex(i, attributes, spaces);
            if (!vertex) {
                return std::unexpected(std::move(vertex.error()));
            }
            geometry.vertices.push_back(*vertex);
        }
        if (attributes.colors != nullptr) {
            model_.vertices = GdbVertices::Colored;
        }
        return AddTriangles(primitive, attributes.positions->count, first);
    }

    static Result<GeometryVertex> BuildVertex(cgltf_size index, const Attributes& attributes,
                                              const MeshSpaces& spaces) {
        const bool isSkinned =
            spaces.skin && attributes.joints != nullptr && attributes.weights != nullptr;
        return (isSkinned ? HeaviestJoint(index, attributes, *spaces.skin)
                          : Result<const JointSpace*>(&spaces.rigid))
            .and_then([&](const JointSpace* space) -> Result<GeometryVertex> {
                const GeometryVertex vertex = ReadVertex(index, *space, attributes);
                if (!IsFinite(vertex.position) || !IsFinite(vertex.normal)) {
                    return Fail(ErrorCode::InvalidData,
                                "a glTF vertex has no finite position or normal");
                }
                return vertex;
            });
    }

    Result<void> AddTriangles(const cgltf_primitive& primitive, cgltf_size vertexCount,
                              std::uint32_t first) {
        GeometryAsset& geometry = model_.geometry;
        const auto start = static_cast<std::uint32_t>(geometry.indices.size());
        const cgltf_size count =
            primitive.indices != nullptr ? primitive.indices->count : vertexCount;
        for (cgltf_size i = 0; i + 2 < count; i += 3) {
            for (cgltf_size corner = 0; corner < 3; ++corner) {
                const cgltf_size index =
                    primitive.indices != nullptr
                        ? cgltf_accessor_read_index(primitive.indices, i + corner)
                        : i + corner;
                if (index >= vertexCount) {
                    return Fail(ErrorCode::InvalidData, "a triangle uses a missing vertex");
                }
                geometry.indices.push_back(first + static_cast<std::uint32_t>(index));
            }
        }
        geometry.ranges.push_back(
            {.firstIndex = start,
             .indexCount = static_cast<std::uint32_t>(geometry.indices.size()) - start,
             .material = FindOrAddMaterial(geometry.materials, primitive.material->name)});
        return {};
    }

    GltfModel& model_;
    const GltfJoints& joints_;
};

std::span<cgltf_node* const> SceneRoots(const cgltf_data& data) {
    const cgltf_scene* scene = data.scene != nullptr ? data.scene : data.scenes;
    if (scene == nullptr) {
        return {};
    }
    return {scene->nodes, scene->nodes_count};
}

}

Result<GltfModel> ImportGltfModel(std::span<const std::byte> bytes, const GltfTarget& target) {
    Result<GltfData> data = ParseGlb(bytes);
    if (!data) {
        return std::unexpected(std::move(data.error()));
    }
    if (target.joints.size() > kMaxJoints) {
        return Fail(ErrorCode::InvalidData, "the model's skeleton has too many joints");
    }
    GltfModel model;
    model.geometry.scale = target.scale;
    const GltfJoints joints(**data, target.joints);
    Flattener flattener(model, joints);
    if (Result<void> added = flattener.AddScene(SceneRoots(**data)); !added) {
        return std::unexpected(std::move(added.error()));
    }
    if (model.geometry.ranges.empty()) {
        return Fail(ErrorCode::InvalidData, "the glTF scene has no meshes");
    }
    return model;
}

}
