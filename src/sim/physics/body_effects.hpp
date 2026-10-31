#pragma once

#include <cstddef>

#include "core/vector3.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/physics/chassis.hpp"

namespace rr::sim {

void StartSpin(BodyState& body, const BodyConstants& constants, float turns, float ratePerMs,
               float align);

void FinishSpin(BodyState& body);
void EndSpin(BodyState& body);

/// Returns the ms this step runs, which is less than `dtMs` on a spin's last step.
[[nodiscard]] int CountDownSpin(BodyState& body, int dtMs);

void StartSpinOut(BodyState& body);
void EndSpinOut(BodyState& body);
void CountDownSpinOut(BodyState& body, int dtMs);

void ApplyLaunch(BodyState& body, const BodyConstants& constants, float strength);

void StartHover(BodyState& body, const BodyConstants& constants);
void StopHover(BodyState& body);

void StartExternalForce(BodyState& body, const BodyConstants& constants, std::size_t slot,
                        const Vector3& force);
void EndExternalForce(BodyState& body, std::size_t slot);

}
