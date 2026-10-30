#include "app/car_sounds.hpp"

#include <algorithm>
#include <cmath>

namespace rr::app {
namespace {

constexpr float kEngineVolume = 0.7F;
constexpr float kFinishedEngineVolume = 0.5F;
constexpr float kEngineRampPerMs = 0.03F * 0.06F;
constexpr float kIdleRampScale = 1.5F;
constexpr float kEngineCurve = 2.2439947F;
constexpr float kCoastSpeed = 0.015F;
constexpr float kPitchSpeed = 0.17F;
constexpr float kBasePitch = 0.4F;
constexpr float kDrivePitchScale = 0.5F;
constexpr float kCoastPitchScale = 0.6F;
constexpr int kAirborneMs = 50;
constexpr float kAirbornePitch = 0.1F;
constexpr float kOpponentAirbornePitch = 0.2F;
constexpr float kBrakeSpeed = 0.01F;
constexpr float kSurfaceSpeed = 0.009F;
constexpr float kSurfacePitchSpeed = 0.22F;
constexpr float kSurfaceMinPitch = 0.5F;
constexpr float kSurfaceMaxPitch = 2.0F;
constexpr Milliseconds kSurfaceFadeIn{280};

EngineLoop ActiveLoop(const EngineInput& input) {
    if (input.thrust != 0.0F || input.hasTurbo) {
        return EngineLoop::Drive;
    }
    return input.speed > kCoastSpeed ? EngineLoop::Coast : EngineLoop::Idle;
}

float SpeedPitch(float speed, float scale, float pitchScale) {
    return kBasePitch + (speed / kPitchSpeed * scale * pitchScale);
}

float LoopPitch(EngineLoop loop, const EngineInput& input) {
    switch (loop) {
        case EngineLoop::Idle:
            return std::clamp(input.pitchScale, 0.0F, 1.0F);
        case EngineLoop::Drive: {
            const float airborne = input.airborneMs > kAirborneMs ? kAirbornePitch : 0.0F;
            return std::clamp(
                SpeedPitch(input.speed, kDrivePitchScale, input.pitchScale) + airborne, 0.0F, 1.0F);
        }
        case EngineLoop::Coast:
            return std::clamp(SpeedPitch(input.speed, kCoastPitchScale, input.pitchScale), 0.0F,
                              1.0F);
    }
    return 1.0F;
}

float LoopVolume(float level, float target) {
    if (level <= 0.0F || level >= target) {
        return level <= 0.0F ? 0.0F : target;
    }
    return target * std::sin(level * kEngineCurve);
}

}

std::array<audio::VoiceMix, kEngineLoopCount> EngineSound::Update(const EngineInput& input,
                                                                  Milliseconds elapsed) {
    if (!isRunning_) {
        constexpr audio::VoiceMix kSilent{.volume = 0.0F};
        return {kSilent, kSilent, kSilent};
    }
    const float target = input.hasFinished ? kFinishedEngineVolume : kEngineVolume;
    const float step = static_cast<float>(elapsed.count()) * kEngineRampPerMs;
    const EngineLoop active = ActiveLoop(input);
    std::array<audio::VoiceMix, kEngineLoopCount> mixes{};
    for (std::size_t i = 0; i < kEngineLoopCount; ++i) {
        const auto loop = static_cast<EngineLoop>(i);
        float& level = levels_.at(i);
        if (loop == active) {
            const float rise = loop == EngineLoop::Idle ? step * kIdleRampScale : step;
            level = std::min(level + rise, target);
        } else {
            level = std::max(level - step, 0.0F);
        }
        mixes.at(i) = {.volume = LoopVolume(level, target), .pitch = LoopPitch(loop, input)};
    }
    return mixes;
}

float OpponentEnginePitch(float speed, int airborneMs, float pitchScale) {
    const float pitch = std::clamp(SpeedPitch(speed, kDrivePitchScale, pitchScale), 0.0F, 1.0F);
    return airborneMs > kAirborneMs ? pitch + kOpponentAirbornePitch : pitch;
}

BrakeSound Brakes::Update(const sim::BodyState& body) {
    const bool wasBraking = isBraking_;
    const float forward = body.derived.forwardSpeed;
    isBraking_ =
        body.thrust < 0.0F && (wasBraking ? forward >= kBrakeSpeed : forward > kBrakeSpeed);
    if (!isBraking_) {
        return {};
    }
    return {.hasStarted = !wasBraking, .loopPitch = 1.0F - (kPitchSpeed - body.derived.speed)};
}

std::optional<SurfaceLoop> SurfaceSound::Update(const sim::BodyState& body, Milliseconds elapsed) {
    const auto touched = std::ranges::find_if(body.motion.wheels, [](const sim::WheelProbe& wheel) {
        return wheel.isInContact && wheel.surface.soundId.has_value();
    });
    const float speed = std::abs(body.derived.speed);
    if (touched == body.motion.wheels.end() || speed < kSurfaceSpeed) {
        soundId_.reset();
        return std::nullopt;
    }
    if (touched->surface.soundId != soundId_) {
        soundId_ = touched->surface.soundId;
        playing_ = Milliseconds{0};
    }
    playing_ += elapsed;
    const float volume = std::min(
        static_cast<float>(playing_.count()) / static_cast<float>(kSurfaceFadeIn.count()), 1.0F);
    const float pitch =
        std::clamp((speed / kSurfacePitchSpeed) + kBasePitch, kSurfaceMinPitch, kSurfaceMaxPitch);
    return SurfaceLoop{.soundId = *soundId_, .mix = {.volume = volume, .pitch = pitch}};
}

}
