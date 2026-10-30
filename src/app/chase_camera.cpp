#include "app/chase_camera.hpp"

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr float kChaseDistance = 7.0F;
constexpr float kChaseHeight = 3.0F;
constexpr float kLookHeight = 1.0F;
constexpr float kChaseBlend = 0.1F;

Vector3 Raised(const Vector3& point, float height) {
    return {.x = point.x, .y = point.y + height, .z = point.z};
}

Vector3 ChaseSpot(const Transform& car) {
    return Raised(Apply(car, ToEngine({.y = -kChaseDistance})), kChaseHeight);
}

}

ChaseCamera::ChaseCamera(const Transform& car) : position_(ChaseSpot(car)) {}

void ChaseCamera::Follow(const Transform& car) {
    position_ = Lerp(position_, ChaseSpot(car), kChaseBlend);
}

void ChaseCamera::JumpTo(const Transform& car) {
    position_ = ChaseSpot(car);
}

Transform ChaseCamera::Pose(const Transform& car) const {
    return LookAt(position_, Raised(car.position, kLookHeight));
}

}
