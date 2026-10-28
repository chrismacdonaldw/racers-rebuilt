#include "sim/physics/racer.hpp"

#include <cmath>

#include "core/assert.hpp"
#include "sim/physics/car_body.hpp"
#include "sim/physics/drive_controller.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr float kBaseMaxSpeed = 120.0F;
constexpr float kMinWorldZ = -250.0F;
constexpr float kMaxWorldZ = 340.0F;
constexpr float kMinDriftSpeed = 0.03F;
constexpr Milliseconds kMaxStep{40};

Basis HeadingBasis(float headingRadians) {
    return OrthonormalBasis({.x = std::cos(headingRadians), .y = std::sin(headingRadians)},
                            {.z = 1.0F});
}

}

Racer::Racer(const ChassisSpec& chassis, const CarBuild& build, const RacerStart& start)
    : constants_(MakeBodyConstants(chassis, build)),
      stats_(MakeStatMultipliers(chassis, build)),
      body_(MakeBody(constants_, start.position, HeadingBasis(start.headingRadians))) {
    SetDrive(body_, constants_, 0.0F, kBaseMaxSpeed * stats_.topSpeed);
}

void Racer::Tick(Milliseconds elapsed, const RacerControls& controls, const CollisionQuery& world) {
    RR_ASSERT(elapsed > Milliseconds{0} && elapsed <= kMaxStep);
    const auto dtMs = static_cast<int>(elapsed.count());
    DriveCommand command = driver_.Update(controls, CanStartDrift(body_), dtMs);
    if (command.isDrifting && body_.derived.forwardSpeed < kMinDriftSpeed) {
        driver_.EndDrift();
        command.isDrifting = false;
        command.isHardDrift = false;
    }
    ApplyDriveCommand(body_, constants_, command, stats_);
    StepBody(body_, constants_, dtMs, world);
    KeepInWorldBounds();
}

Transform Racer::Pose() const {
    return {.position = body_.motion.position, .rotation = ToQuaternion(body_.motion.basis)};
}

void Racer::KeepInWorldBounds() {
    const float z = body_.motion.position.z;
    if (z >= kMinWorldZ && z <= kMaxWorldZ) {
        return;
    }
    body_.motion = body_.saved;
    body_.motion.velocity = {};
}

}
