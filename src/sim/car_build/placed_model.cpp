#include "sim/car_build/placed_model.hpp"

#include <cstddef>
#include <cstdint>

#include "assets/game_path.hpp"
#include "assets/piece_models.hpp"

namespace rr::sim {
namespace {

constexpr float kPlateHeight = 0.4F;

Vector3 Turned(const Vector3& point, unsigned rotation, float width, float depth) {
    switch (rotation & 3U) {
        case 0:
            return point;
        case 1:
            return {.x = point.y, .y = width - point.x, .z = point.z};
        case 2:
            return {.x = width - point.x, .y = depth - point.y, .z = point.z};
        default:
            return {.x = depth - point.y, .y = point.x, .z = point.z};
    }
}

std::string MaterialName(const std::string& name, const PieceInstance& instance,
                         std::span<const std::string> materials) {
    const bool isPieceColour =
        assets::ToUpperAscii(name) == assets::ToUpperAscii(assets::kPieceColourMaterial);
    return isPieceColour && instance.colourMaterial < materials.size()
               ? materials[instance.colourMaterial]
               : name;
}

}

void AppendPlacedModel(assets::GeometryAsset& geometry, const assets::GeometryAsset& model,
                       const PieceInstance& instance, const Vector3& offset,
                       std::span<const std::string> materials) {
    const auto width = static_cast<float>(instance.piece->shape.size.width);
    const auto depth = static_cast<float>(instance.piece->shape.size.height);
    const unsigned rotation = instance.placement.rotation;
    const Vector3 origin{.x = static_cast<float>(instance.placement.x) + offset.x,
                         .y = static_cast<float>(instance.placement.y) + offset.y,
                         .z = (static_cast<float>(instance.height) * kPlateHeight) + offset.z};
    const auto base = static_cast<std::uint32_t>(geometry.vertices.size());
    geometry.vertices.reserve(geometry.vertices.size() + model.vertices.size());
    for (assets::GeometryVertex vertex : model.vertices) {
        vertex.position = Turned(vertex.position, rotation, width, depth) + origin;
        vertex.normal = Turned(vertex.normal, rotation, 0.0F, 0.0F);
        vertex.joint = 0;
        geometry.vertices.push_back(vertex);
    }
    geometry.indices.reserve(geometry.indices.size() + model.indices.size());
    for (const assets::GeometryRange& range : model.ranges) {
        const std::string name =
            MaterialName(model.materials.at(range.material), instance, materials);
        geometry.ranges.push_back(
            {.firstIndex = static_cast<std::uint32_t>(geometry.indices.size()),
             .indexCount = range.indexCount,
             .material = assets::FindOrAddMaterial(geometry.materials, name)});
        for (std::size_t index = range.firstIndex; index < range.firstIndex + range.indexCount;
             ++index) {
            geometry.indices.push_back(base + model.indices.at(index));
        }
    }
}

}
