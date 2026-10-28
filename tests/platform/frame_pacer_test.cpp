#include "platform/frame_pacer.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

#include "platform/clock.hpp"

namespace rr::platform {
namespace {

constexpr int kFps = 144;

FramePacer CappedPacer() {
    Result<FrameTimer> timer = FrameTimer::Create();
    REQUIRE(timer.has_value());
    return {std::move(*timer), {.mode = PresentMode::Capped, .fpsCap = kFps}};
}

TEST_CASE("capped pacing never runs faster than its cap") {
    FramePacer pacer = CappedPacer();
    constexpr int kFrames = 30;

    const Nanoseconds start = Now();
    for (int i = 0; i < kFrames; ++i) {
        pacer.WaitForNextFrame();
    }
    const Nanoseconds elapsed = Now() - start;

    CHECK(elapsed >= Nanoseconds{std::chrono::seconds{1}} * kFrames / kFps);
}

TEST_SUITE("perf") {
    TEST_CASE("capped pacing holds 144 FPS within 0.5 ms of jitter" * doctest::skip()) {
        FramePacer pacer = CappedPacer();

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
        const double deviationMs =
            std::sqrt(squaredError / static_cast<double>(frameTimesMs.size()));

        MESSAGE("mean ", meanMs, " ms, standard deviation ", deviationMs, " ms");
        CHECK(meanMs == doctest::Approx(1000.0 / kFps).epsilon(0.01));
        CHECK(deviationMs < 0.5);
    }
}

}
}
