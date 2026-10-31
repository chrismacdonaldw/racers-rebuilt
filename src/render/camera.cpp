#include "render/camera.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>

namespace rr::render {

glm::mat4 ModelMatrix(const Transform& transform, float scale) {
    const Vector3& p = transform.position;
    const Quaternion& r = transform.rotation;
    return glm::scale(glm::translate(glm::mat4(1.0F), glm::vec3(p.x, p.y, p.z)) *
                          glm::mat4_cast(glm::quat(r.w, r.x, r.y, r.z)),
                      glm::vec3(scale));
}

glm::mat4 ViewProjection(const Camera& camera, float aspectRatio) {
    return glm::perspective(glm::radians(camera.verticalFovDegrees), aspectRatio, camera.nearPlane,
                            camera.farPlane) *
           glm::inverse(ModelMatrix(camera.transform));
}

}
