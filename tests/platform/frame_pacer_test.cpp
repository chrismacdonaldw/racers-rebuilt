#include "platform/frame_pacer.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

#include "platform/clock.hpp"

namespace rr::platform {
namespace {

TEST_CASE("capped pacing holds 144 FPS within 0.5 ms of jitter") {
    constexpr int kFps = 144;
    Result<FrameTimer> timer = FrameTimer::Create();
    REQUIRE(timer.has_value());
    FramePacer pacer(std::move(*timer), {.mode = PresentMode::Capped, .fpsCap = kFps});

    std::vector<double> frameTimesMs;
    pacer.WaitForNextFrame();
    Nanoseconds previous = Now();
    for (int i = 0; i < kFps; ++i) {
        pacer.WaitForNextFrame();
        const Nanoseconds now = Now();
        frameTimesMs.push_back(FloatSeconds{now - previous}.count() * 1000.0);
        previous = now;
    }

    double sum = 0.0;
    for (const double frameTimeMs : frameTimesMs) {
        sum += frameTimeMs;
    }
    const double meanMs = sum / static_cast<double>(frameTimesMs.size());
    double squaredError = 0.0;
    for (const double frameTimeMs : frameTimesMs) {
        squaredError += (frameTimeMs - meanMs) * (frameTimeMs - meanMs);
    }
    const double deviationMs = std::sqrt(squaredError / static_cast<double>(frameTimesMs.size()));

    MESSAGE("mean ", meanMs, " ms, standard deviation ", deviationMs, " ms");
    CHECK(meanMs == doctest::Approx(1000.0 / kFps).epsilon(0.01));
    CHECK(deviationMs < 0.5);
}

}
}
