#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "audio/voice_mix.hpp"
#include "core/duration.hpp"
#include "sim/physics/body_state.hpp"

namespace rr::app {

enum class EngineLoop : std::uint8_t { Idle, Drive, Coast };

inline constexpr std::size_t kEngineLoopCount = 3;

/// Speeds are units per ms. `pitchScale` is the chassis's engine pitch.
struct EngineInput {
    float thrust = 0.0F;
    bool hasTurbo = false;
    float speed = 0.0F;
    int airborneMs = 0;
    bool hasFinished = false;
    float pitchScale = 1.0F;
};

class EngineSound {
public:
    void Start() { isRunning_ = true; }

    /// One mix per `EngineLoop`. A loop at volume 0 is stopped, and every loop is until `Start`.
    [[nodiscard]] std::array<audio::VoiceMix, kEngineLoopCount> Update(const EngineInput& input,
                                                                       Milliseconds elapsed);

private:
    std::array<float, kEngineLoopCount> levels_{};
    bool isRunning_ = false;
};

[[nodiscard]] float OpponentEnginePitch(float speed, int airborneMs, float pitchScale);

struct BrakeSound {
    bool hasStarted = false;
    std::optional<float> loopPitch;
};

class Brakes {
public:
    [[nodiscard]] BrakeSound Update(const sim::BodyState& body);

private:
    bool isBraking_ = false;
};

struct SurfaceLoop {
    std::int32_t soundId = 0;
    audio::VoiceMix mix;
};

class SurfaceSound {
public:
    /// Empty while the racer makes no surface sound.
    [[nodiscard]] std::optional<SurfaceLoop> Update(const sim::BodyState& body,
                                                    Milliseconds elapsed);

private:
    std::optional<std::int32_t> soundId_;
    Milliseconds playing_{0};
};

}
