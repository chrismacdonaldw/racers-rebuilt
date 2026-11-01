#include "sim/hazard/ufo_grabber.hpp"

#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kBeamNode = 0;
constexpr float kBeamRadius = 45.0F;
constexpr Milliseconds kHoldTime{1'000};
constexpr Milliseconds kLockoutTime{1'000};
constexpr Milliseconds kWatchdogTime{500};

bool CountDown(Milliseconds& left, Milliseconds elapsed) {
    if (elapsed < left) {
        left -= elapsed;
        return false;
    }
    left = Milliseconds{0};
    return true;
}

}

void UfoGrabber::Tick(HazardContext& context) {
    if (!entity) {
        return;
    }
    reach.centre = context.entities->NodeWorld(*entity, kBeamNode).position;
    if (watchdog > Milliseconds{0} && CountDown(watchdog, context.elapsed)) {
        Release(context);
        phase = GrabPhase::Free;
        phaseLeft = Milliseconds{0};
    }
    if (phaseLeft > Milliseconds{0} && CountDown(phaseLeft, context.elapsed)) {
        if (phase == GrabPhase::Holding) {
            Release(context);
            watchdog = Milliseconds{0};
            phase = GrabPhase::Lockout;
            phaseLeft = kLockoutTime;
        } else if (phase == GrabPhase::Lockout) {
            phase = GrabPhase::Free;
        }
    }
}

void UfoGrabber::Touch(HazardContext& context) {
    if (!entity) {
        return;
    }
    for (std::size_t i = 0; i < context.racers.size(); ++i) {
        if (Touches(reach, context.reaches[i])) {
            Grab(i, context);
        }
    }
}

void UfoGrabber::Grab(std::size_t racer, HazardContext& context) {
    const float frame = context.entities->Frame(*entity);
    Racer& car = context.racers[racer];
    if (phase == GrabPhase::Lockout || (lowFrame < frame && frame < highFrame) ||
        car.IsShielded() || (held && *held != racer)) {
        return;
    }
    if (!held) {
        phase = GrabPhase::Holding;
        phaseLeft = kHoldTime;
    }
    held = static_cast<std::uint8_t>(racer);
    Vector3 toBeam = reach.centre - car.Pose().position;
    toBeam.z = 0.0F;
    const float distance = Length(toBeam);
    car.StartGrab(distance > 0.0F ? toBeam * (pull / distance) : Vector3{});
    watchdog = kWatchdogTime;
}

void UfoGrabber::Release(HazardContext& context) {
    if (held) {
        context.racers[*held].EndGrab();
        held.reset();
    }
}

UfoGrabber LoadUfoGrabber(const assets::UfoGrabber& payload, const TrackEntities& entities) {
    return {.entity = entities.Find(payload.entity),
            .pull = payload.pullStrength,
            .lowFrame = payload.grabFrameLow,
            .highFrame = payload.grabFrameHigh,
            .reach = {.radius = kBeamRadius}};
}

}
