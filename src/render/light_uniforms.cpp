#include "render/light_uniforms.hpp"

#include <cstddef>

#include "core/assert.hpp"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

namespace rr::render {

LightUniforms ToLightUniforms(const Lighting& lighting) {
    RR_VERIFY(lighting.lights.size() <= kMaxDirectionalLights);
    LightUniforms uniforms{.count = static_cast<std::uint32_t>(lighting.lights.size())};
    for (std::size_t i = 0; i < lighting.lights.size(); ++i) {
        const DirectionalLight& light = lighting.lights[i];
        const glm::vec3 direction(light.direction.x, light.direction.y, light.direction.z);
        if (glm::dot(direction, direction) == 0.0F) {
            continue;
        }
        uniforms.directions.at(i) = glm::vec4(glm::normalize(direction), 0.0F);
        uniforms.colors.at(i) = glm::vec4(light.color.r, light.color.g, light.color.b, 1.0F);
    }
    return uniforms;
}

}
