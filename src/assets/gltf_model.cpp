#include "assets/gltf_model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cgltf.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
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

using GltfData = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;

struct NodeSpace {
    glm::mat4 position{1.0F};
    glm::mat3 normal{1.0F};
};

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
};

Vector3 ToGame(const glm::vec3& gltf) {
    return {.x = gltf.x, .y = -gltf.z, .z = gltf.y};
}

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

bool HasEmbeddedBuffers(const cgltf_data& data) {
    return std::ranges::all_of(
        std::span(data.buffers, data.buffers_count), [](const cgltf_buffer& buffer) {
            return buffer.uri == nullptr || std::string_view(buffer.uri).starts_with("data:");
        });
}

Result<GltfData> Parse(std::span<const std::byte> bytes) {
    const cgltf_options options{};
    cgltf_data* raw = nullptr;
    cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &raw);
    GltfData data(raw, cgltf_free);
    if (result == cgltf_result_success && !HasEmbeddedBuffers(*data)) {
        result = cgltf_result_file_not_found;
    }
    if (result == cgltf_result_success) {
        result = cgltf_load_buffers(&options, data.get(), nullptr);
    }
    if (result == cgltf_result_success) {
        result = cgltf_validate(data.get());
    }
    if (result != cgltf_result_success) {
        return Fail(ErrorCode::InvalidData,
                    std::format("not a self-contained glTF binary (cgltf error {})",
                                static_cast<int>(result)));
    }
    return data;
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
            .colors = cgltf_find_accessor(&primitive, cgltf_attribute_type_color, 0)};
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
        AccessorRule{.accessor = attributes.positions,
                     .type = cgltf_type_vec3,
                     .alternative = cgltf_type_vec3},
        AccessorRule{.accessor = attributes.normals,
                     .type = cgltf_type_vec3,
                     .alternative = cgltf_type_vec3},
        AccessorRule{
            .accessor = attributes.uvs, .type = cgltf_type_vec2, .alternative = cgltf_type_vec2},
        AccessorRule{
            .accessor = attributes.colors, .type = cgltf_type_vec3, .alternative = cgltf_type_vec4},
        AccessorRule{.accessor = primitive.indices,
                     .type = cgltf_type_scalar,
                     .alternative = cgltf_type_scalar}};
    for (const AccessorRule& rule : rules) {
        checked = checked.and_then([&rule] { return CheckAccessor(rule); });
    }
    return checked;
}

GeometryVertex ReadVertex(cgltf_size index, const NodeSpace& space, const Attributes& attributes) {
    GeometryVertex vertex;
    vertex.position =
        ToGame(glm::vec3(space.position * Read<glm::vec4>(attributes.positions, index)));
    if (attributes.normals != nullptr) {
        vertex.normal =
            ToGame(glm::normalize(space.normal * Read<glm::vec3>(attributes.normals, index)));
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

class Flattener {
public:
    explicit Flattener(GltfModel& model) : model_(model) {}

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
    Result<void> AddMesh(const cgltf_node& node) {
        if (node.mesh == nullptr) {
            return {};
        }
        std::array<float, 16> world{};
        cgltf_node_transform_world(&node, world.data());
        const glm::mat4 position = glm::make_mat4(world.data());
        const NodeSpace space{.position = position,
                              .normal = glm::inverseTranspose(glm::mat3(position))};
        for (const cgltf_primitive& primitive :
             std::span(node.mesh->primitives, node.mesh->primitives_count)) {
            Result<void> added = AddPrimitive(primitive, space);
            if (!added) {
                return added;
            }
        }
        return {};
    }

    Result<void> AddPrimitive(const cgltf_primitive& primitive, const NodeSpace& space) {
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
            const GeometryVertex vertex = ReadVertex(i, space, attributes);
            if (!IsFinite(vertex.position) || !IsFinite(vertex.normal)) {
                return Fail(ErrorCode::InvalidData,
                            "a glTF vertex has no finite position or normal");
            }
            geometry.vertices.push_back(vertex);
        }
        if (attributes.colors != nullptr) {
            model_.vertices = GdbVertices::Colored;
        }
        return AddTriangles(primitive, attributes.positions->count, first);
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
};

std::span<cgltf_node* const> SceneRoots(const cgltf_data& data) {
    const cgltf_scene* scene = data.scene != nullptr ? data.scene : data.scenes;
    if (scene == nullptr) {
        return {};
    }
    return {scene->nodes, scene->nodes_count};
}

}

Result<GltfModel> ImportGltfModel(std::span<const std::byte> bytes) {
    Result<GltfData> data = Parse(bytes);
    if (!data) {
        return std::unexpected(std::move(data.error()));
    }
    GltfModel model;
    Flattener flattener(model);
    if (Result<void> added = flattener.AddScene(SceneRoots(**data)); !added) {
        return std::unexpected(std::move(added.error()));
    }
    if (model.geometry.ranges.empty()) {
        return Fail(ErrorCode::InvalidData, "the glTF scene has no meshes");
    }
    return model;
}

}
