#pragma once

#include <array>
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

    friend constexpr Color operator*(const Color& x, const Color& y) {
        return {.r = x.r * y.r, .g = x.g * y.g, .b = x.b * y.b, .a = x.a * y.a};
    }
};

/// In texture widths and heights, added to the mesh's own texture coordinates.
struct UvOffset {
    float u = 0.0F;
    float v = 0.0F;

    friend bool operator==(const UvOffset&, const UvOffset&) = default;
};

/// Each vertex color channel becomes `min(1, color * scale + offset)`.
struct VertexColorTransform {
    Color scale;
    Color offset{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.0F};
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
    UvOffset uvOffset;
    VertexColorTransform vertexColors;
};

struct MeshDraw {
    MeshId mesh{};
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = kWholeMesh;
    Material material;
    Transform transform;
    float scale = 1.0F;
    bool isStatic = false;
};

/// One directional light, in engine space. `ambient` scales each material's own ambient.
struct Lighting {
    Vector3 direction{.x = -0.36F, .y = -0.86F, .z = -0.36F};
    Color ambient;
};

struct DebugLine {
    Vector3 from;
    Vector3 to;
    Color color;
};

struct Rect {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
};

/// `bounds` is in UI coordinates and `uv` in texture coordinates.
struct SpriteDraw {
    TextureId texture = kWhiteTexture;
    Rect bounds;
    Rect uv{.width = 1.0F, .height = 1.0F};
    Color tint;
    BlendMode blend = BlendMode::Alpha;
};

/// Corners go top left, top right, bottom left, bottom right, matching `uv`.
struct QuadDraw {
    TextureId texture = kWhiteTexture;
    std::array<Vector3, 4> corners{};
    Rect uv{.width = 1.0F, .height = 1.0F};
    Color tint;
    std::array<Color, 4> cornerTints{};
    BlendMode blend = BlendMode::Alpha;
};

/// In fractions of the window's width and height, from its top left.
struct Viewport {
    float x = 0.0F;
    float y = 0.0F;
    float width = 1.0F;
    float height = 1.0F;
};

/// Quads draw after the meshes without writing depth, then sprites over the scene, each in order.
struct RenderView {
    Camera camera;
    Lighting lighting;
    std::span<const MeshDraw> meshes;
    std::span<const QuadDraw> quads;
    std::span<const DebugLine> lines;
    std::span<const SpriteDraw> sprites;
    Viewport viewport;
};

}
