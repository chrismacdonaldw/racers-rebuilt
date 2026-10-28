#pragma once

#include <cstdint>
#include <limits>
#include <span>

#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/camera.hpp"

namespace rr::render {

enum class MeshId : std::uint32_t {};
enum class TextureId : std::uint32_t {};

/// Always uploaded first, so a default `Material` is untextured.
inline constexpr TextureId kWhiteTexture{};
inline constexpr std::uint32_t kWholeMesh = std::numeric_limits<std::uint32_t>::max();

/// Opaque draws come first. Blended draws follow in submission order and don't write depth.
enum class BlendMode : std::uint8_t { Opaque, Alpha, Additive };

struct Color {
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

/// Lit like fixed-function Direct3D: `ambient` plus `tint` by the light, or the vertex color where
/// a vertex has no normal. Without `modulates`, the texture is drawn unlit as is.
struct Material {
    TextureId texture = kWhiteTexture;
    Color tint;
    Color ambient{.r = 0.35F, .g = 0.35F, .b = 0.35F};
    float alphaCutoff = 0.5F;
    BlendMode blend = BlendMode::Opaque;
    bool modulates = true;
};

struct MeshDraw {
    MeshId mesh{};
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = kWholeMesh;
    Material material;
    Transform transform;
};

struct DebugLine {
    Vector3 from;
    Vector3 to;
    Color color;
};

struct RenderView {
    Camera camera;
    std::span<const MeshDraw> meshes;
    std::span<const DebugLine> lines;
};

}
