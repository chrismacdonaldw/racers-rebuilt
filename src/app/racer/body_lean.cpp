#include "app/racer/body_lean.hpp"

#include "sim/physics/facing.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

constexpr float kPitchStiffness = 30.0F;
constexpr float kRollStiffness = 100.0F;
constexpr float kPitchDecayPerMs = 0.01F;
constexpr float kRollDecayPerMs = 0.03F;

float SignedError(const Vector3& kept, const Vector3& actual, const Vector3& up) {
    const float error = Length(kept - actual);
    return Dot(kept, up) < 0.0F ? -error : error;
}

float Decayed(float value, float decayPerMs, float elapsedMs) {
    const float factor = 1.0F - (decayPerMs * elapsedMs);
    return factor > 0.0F ? value * factor : 0.0F;
}

Vector3 Leaned(const Vector3& kept, const Vector3& actual, const Vector3& up, float lean) {
    return lean == 0.0F ? actual : kept + (lean * up);
}

}

Transform BodyLean::Update(const Transform& wheels, float elapsedMs, float mass, float driftRoll) {
    const Vector3 forward = Rotate(wheels.rotation, {.x = 1.0F});
    const Vector3 side = Rotate(wheels.rotation, {.y = 1.0F});
    const Vector3 up = Rotate(wheels.rotation, {.z = 1.0F});
    const Vector3 previousUp = up_.value_or(up);
    const Vector3 keptForward = Cross(side, previousUp);
    const Vector3 keptSide = Cross(previousUp, forward);
    const float step = elapsedMs / mass;
    pitch_ -= kPitchStiffness * SignedError(keptForward, forward, up) * step;
    roll_ -= kRollStiffness * SignedError(keptSide, side, up) * step;
    roll_ += driftRoll * elapsedMs / sim::kDriftRollStepMs;
    pitch_ = Decayed(pitch_, kPitchDecayPerMs, elapsedMs);
    roll_ = Decayed(roll_, kRollDecayPerMs, elapsedMs);
    const Vector3 bodyForward = Leaned(keptForward, forward, up, pitch_);
    const Vector3 bodySide = Leaned(keptSide, side, up, roll_);
    const sim::Basis basis = sim::OrthonormalBasis(bodyForward, Cross(bodyForward, bodySide));
    up_ = basis.up;
    return {.position = wheels.position, .rotation = sim::ToQuaternion(basis)};
}

}
