#include "app/car_sounds.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace rr::app {
namespace {

constexpr Milliseconds kTick{12};

std::array<audio::VoiceMix, kEngineLoopCount> Run(EngineSound& engine, const EngineInput& input,
                                                  int ticks) {
    std::array<audio::VoiceMix, kEngineLoopCount> mixes{};
    for (int tick = 0; tick < ticks; ++tick) {
        mixes = engine.Update(input, kTick);
    }
    return mixes;
}

float Volume(const std::array<audio::VoiceMix, kEngineLoopCount>& mixes, EngineLoop loop) {
    return mixes.at(static_cast<std::size_t>(loop)).volume;
}

float Pitch(const std::array<audio::VoiceMix, kEngineLoopCount>& mixes, EngineLoop loop) {
    return mixes.at(static_cast<std::size_t>(loop)).pitch;
}

sim::BodyState Moving(float speed, float thrust = 0.0F) {
    sim::BodyState body;
    body.derived.speed = speed;
    body.derived.forwardSpeed = speed;
    body.thrust = thrust;
    return body;
}

TEST_CASE("an engine stays silent until it starts") {
    EngineSound engine;

    const auto before = Run(engine, {.thrust = 1.0F}, 25);

    CHECK(Volume(before, EngineLoop::Idle) == 0.0F);
    CHECK(Volume(before, EngineLoop::Drive) == 0.0F);
    CHECK(Volume(before, EngineLoop::Coast) == 0.0F);
}

TEST_CASE("a standing engine fades its idle loop in and leaves the others stopped") {
    EngineSound engine;
    engine.Start();

    const auto first = engine.Update({}, kTick);
    const auto settled = Run(engine, {}, 25);

    CHECK(Volume(first, EngineLoop::Idle) ==
          doctest::Approx(0.7F * std::sin(0.0324F * 2.2439947F)));
    CHECK(Volume(settled, EngineLoop::Idle) == 0.7F);
    CHECK(Pitch(settled, EngineLoop::Idle) == 1.0F);
    CHECK(Volume(settled, EngineLoop::Drive) == 0.0F);
    CHECK(Volume(settled, EngineLoop::Coast) == 0.0F);
}

TEST_CASE("throttle crossfades the engine from idle to drive") {
    EngineSound engine;
    engine.Start();
    static_cast<void>(Run(engine, {}, 25));
    const EngineInput throttle{.thrust = 1.0F, .speed = 0.05F};

    const auto blended = Run(engine, throttle, 1);
    const auto driving = Run(engine, throttle, 40);

    CHECK(Volume(blended, EngineLoop::Drive) > 0.0F);
    CHECK(Volume(blended, EngineLoop::Idle) > Volume(blended, EngineLoop::Drive));
    CHECK(Volume(driving, EngineLoop::Drive) == 0.7F);
    CHECK(Volume(driving, EngineLoop::Idle) == 0.0F);
}

TEST_CASE("engine pitch follows speed and airtime, up to 1") {
    EngineSound engine;
    engine.Start();

    CHECK(Pitch(engine.Update({.thrust = 1.0F, .speed = 0.17F}, kTick), EngineLoop::Drive) ==
          doctest::Approx(0.9F));
    CHECK(Pitch(engine.Update({.thrust = 1.0F, .speed = 0.17F, .airborneMs = 51}, kTick),
                EngineLoop::Drive) == doctest::Approx(1.0F));
    CHECK(Pitch(engine.Update({.speed = 0.17F}, kTick), EngineLoop::Coast) ==
          doctest::Approx(1.0F));
    CHECK(Pitch(engine.Update({.speed = 0.085F}, kTick), EngineLoop::Coast) ==
          doctest::Approx(0.7F));
    CHECK(OpponentEnginePitch(0.34F, 51, 1.0F) == doctest::Approx(1.2F));
}

TEST_CASE("a finished racer's engine settles at half volume") {
    EngineSound engine;
    engine.Start();

    const auto mixes = Run(engine, {.hasFinished = true}, 25);

    CHECK(Volume(mixes, EngineLoop::Idle) == 0.5F);
}

TEST_CASE("brakes squeal from speed until the car slows or the brake lifts") {
    Brakes brakes;

    const BrakeSound start = brakes.Update(Moving(0.1F, -1.0F));
    const BrakeSound held = brakes.Update(Moving(0.1F, -1.0F));

    CHECK(start.hasStarted);
    CHECK(start.loopPitch == doctest::Approx(0.93F));
    CHECK_FALSE(held.hasStarted);
    CHECK(held.loopPitch.has_value());
    CHECK_FALSE(brakes.Update(Moving(0.005F, -1.0F)).loopPitch.has_value());
    CHECK_FALSE(brakes.Update(Moving(0.1F)).loopPitch.has_value());
}

sim::BodyState OnSoundSurface(float speed) {
    sim::BodyState body = Moving(speed);
    body.motion.wheels[2].isInContact = true;
    body.motion.wheels[2].surface.soundId = 3002;
    return body;
}

SurfaceLoop Playing(const std::optional<SurfaceLoop>& loop) {
    return loop.value_or(SurfaceLoop{.soundId = -1});
}

TEST_CASE("a surface loop fades in on a sound surface") {
    SurfaceSound surface;
    const sim::BodyState body = OnSoundSurface(0.11F);

    const SurfaceLoop start = Playing(surface.Update(body, kTick));
    SurfaceLoop later;
    for (int tick = 0; tick < 30; ++tick) {
        later = Playing(surface.Update(body, kTick));
    }

    CHECK(start.soundId == 3002);
    CHECK(start.mix.volume == doctest::Approx(12.0F / 280.0F));
    CHECK(start.mix.pitch == doctest::Approx(0.9F));
    CHECK(later.mix.volume == 1.0F);
}

TEST_CASE("a surface loop stops off its surface or when slow") {
    SurfaceSound surface;
    static_cast<void>(surface.Update(OnSoundSurface(0.11F), kTick));

    CHECK_FALSE(surface.Update(Moving(0.11F), kTick).has_value());
    CHECK_FALSE(surface.Update(OnSoundSurface(0.005F), kTick).has_value());
}

}
}
