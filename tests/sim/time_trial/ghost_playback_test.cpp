#include "sim/time_trial/ghost_playback.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <vector>

#include "ghost_run_rig.hpp"
#include "sim/time_trial/ghost_recorder.hpp"

namespace rr::sim {
namespace {

using test::At;
using test::FrameOf;
using test::Laps;
using test::StraightGhost;

constexpr Milliseconds kTick{12};

std::vector<std::int64_t> SampledTimes(Milliseconds step, Milliseconds length) {
    GhostRecorder recorder;
    std::vector<std::int64_t> times;
    for (Milliseconds time = step; time <= length; time += step) {
        const std::size_t before = recorder.Run().samples.size();
        recorder.Record(step, At(0.0F));
        if (recorder.Run().samples.size() > before) {
            times.push_back(time.count());
        }
    }
    return times;
}

TEST_CASE("a recorder samples the first tick, then the first tick at or after each 250 ms") {
    CHECK(SampledTimes(kTick, Milliseconds{1020}) ==
          std::vector<std::int64_t>{12, 252, 504, 756, 1008});
    CHECK(SampledTimes(Milliseconds{40}, Milliseconds{800}) ==
          std::vector<std::int64_t>{40, 280, 520, 760});
}

TEST_CASE("a recorder keeps the exact start pose and quantized samples") {
    GhostRecorder recorder;
    const Transform start = At(1.01F);

    recorder.Record(kTick, start);
    recorder.Record(kTick, At(9.0F));

    REQUIRE(recorder.Run().samples.size() == 1);
    CHECK(recorder.Run().start == start);
    CHECK(recorder.Run().samples[0] == assets::ToGhostSample(start));
}

void RecordFiveMinutes(GhostRecorder& recorder) {
    for (Milliseconds time{0}; time < Milliseconds{300000}; time += kTick) {
        recorder.Record(kTick, At(1.0F));
    }
}

TEST_CASE("a recorder stops sampling at 961 samples and starts over after taking the run") {
    GhostRecorder recorder;
    RecordFiveMinutes(recorder);
    CHECK(recorder.Run().samples.size() == assets::kGhostSampleCapacity);

    const assets::GhostRun taken = recorder.Take(Laps(1, 2, 3));

    CHECK(taken.samples.size() == assets::kGhostSampleCapacity);
    CHECK(assets::RaceTime(taken) == Milliseconds{6});
    CHECK(recorder.Run().samples.empty());
    recorder.Record(kTick, At(2.0F));
    CHECK(recorder.Run().samples.size() == 1);
}

TEST_CASE("playback lands on each sample every 250 ms and blends between them") {
    const assets::GhostRun run = StraightGhost(4, Laps(0, 0, 0));

    const GhostFrame onSample = FrameOf(GhostFrameAt(run, Milliseconds{500}));
    const GhostFrame halfway = FrameOf(GhostFrameAt(run, Milliseconds{625}));

    CHECK(onSample.pose.position == Vector3{.x = 2.0F});
    CHECK(halfway.pose.position == Vector3{.x = 2.5F});
    CHECK(halfway.pose.rotation == Quaternion{});
    CHECK(halfway.chassisAnimationRate == doctest::Approx(7.2F));
}

TEST_CASE("playback ends when there's no next sample to blend towards") {
    const assets::GhostRun run = StraightGhost(4, Laps(0, 0, 0));

    CHECK(GhostFrameAt(run, Milliseconds{749}).has_value());
    CHECK_FALSE(GhostFrameAt(run, Milliseconds{750}).has_value());
    CHECK_FALSE(GhostFrameAt(StraightGhost(1, Laps(0, 0, 0)), Milliseconds{0}).has_value());
}

TEST_CASE("a recorded run plays back where the car was") {
    GhostRecorder recorder;
    Milliseconds time{0};
    for (int tick = 0; tick < 100; ++tick) {
        time += kTick;
        recorder.Record(kTick, At(static_cast<float>(time.count()) / 100.0F));
    }
    const assets::GhostRun run = recorder.Take(Laps(0, 0, 0));

    const GhostFrame frame = FrameOf(GhostFrameAt(run, Milliseconds{500}));

    CHECK(frame.pose.position.x == doctest::Approx(5.04F).epsilon(0.01));
}

}
}
