#pragma once

#include <array>
#include <cstdint>

#include "render/render_view.hpp"

#include <glm/vec4.hpp>

namespace rr::render {

/// Laid out as `LightUniforms` in `mesh.frag.hlsl`.
struct LightUniforms {
    std::array<glm::vec4, kMaxDirectionalLights> directions{};
    std::array<glm::vec4, kMaxDirectionalLights> colors{};
    std::uint32_t count = 0;
    std::array<std::uint32_t, 3> padding{};
};

/// A light with no direction is kept, unlit, so the count still matches the view's.
[[nodiscard]] LightUniforms ToLightUniforms(const Lighting& lighting);

}
