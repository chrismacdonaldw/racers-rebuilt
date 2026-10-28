#pragma once

#include <cstdint>
#include <span>

#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/camera.hpp"

namespace rr::render {

enum class MeshId : std::uint32_t {};
enum class TextureId : std::uint32_t {};

struct Color {
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

struct Material {
    TextureId texture{};
    Color tint;
    float alphaCutoff = 0.5F;
};

struct MeshDraw {
    MeshId mesh{};
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
