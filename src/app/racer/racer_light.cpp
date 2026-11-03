#include "app/racer/racer_light.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "assets/game_path.hpp"
#include "core/color.hpp"
#include "core/vector3.hpp"

namespace rr::app {
namespace {

constexpr float kChannelMax = 255.0F;
constexpr float kAmbient = 128.0F / kChannelMax;
constexpr Vector3 kLightTravel{.x = -1.0F, .y = 0.0F, .z = -1.0F};

float Unit(std::uint8_t channel) {
    return static_cast<float>(channel) / kChannelMax;
}

std::uint8_t LitChannel(std::uint8_t ambient, std::uint8_t diffuse, float facing) {
    const float lit = std::min(1.0F, (kAmbient * Unit(ambient)) + (Unit(diffuse) * facing));
    return static_cast<std::uint8_t>(std::lround(lit * kChannelMax));
}

Rgba8 Lit(const assets::MaterialAsset& material, const Vector3& normal) {
    const Vector3 toLight = -Normalized(kLightTravel, {});
    const float facing = std::max(0.0F, Dot(Normalized(normal, {}), toLight));
    Rgba8 color = kOpaqueWhite;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        color[channel] = LitChannel(material.ambient[channel], material.diffuse[channel], facing);
    }
    return color;
}

Vector3 Placed(const assets::GeometryVertex& vertex, std::span<const Transform> joints) {
    return vertex.joint < joints.size() ? Rotate(joints[vertex.joint].rotation, vertex.normal)
                                        : vertex.normal;
}

}

void BakeBuiltRacerLight(assets::GeometryAsset& geometry,
                         const assets::NameMap<assets::MaterialAsset>& materials,
                         std::span<const Transform> joints) {
    std::vector<bool> isLit(geometry.vertices.size());
    for (const assets::GeometryRange& range : geometry.ranges) {
        const auto material =
            materials.find(assets::ToUpperAscii(geometry.materials[range.material]));
        if (material == materials.end()) {
            continue;
        }
        const auto indices =
            std::span(geometry.indices).subspan(range.firstIndex, range.indexCount);
        for (const std::uint32_t index : indices) {
            if (!isLit[index]) {
                isLit[index] = true;
                assets::GeometryVertex& vertex = geometry.vertices[index];
                vertex.rgba = Lit(material->second, Placed(vertex, joints));
            }
        }
    }
    for (assets::GeometryVertex& vertex : geometry.vertices) {
        vertex.normal = {};
    }
}

}
