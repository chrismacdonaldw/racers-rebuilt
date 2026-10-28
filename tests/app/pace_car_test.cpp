#include "app/pace_car.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

#include "app/frame_loop.hpp"
#include "app/track_space.hpp"
#include "frame_times.hpp"

namespace rr::app {
namespace {

assets::RoutePoint Turn(float x, float y, float headingRadians, std::int32_t durationMs) {
    return {.offset = {.x = x, .y = y},
            .rotation = FromAxisAngle({.z = 1.0F}, headingRadians),
            .durationMs = durationMs};
}

assets::RaceRoute Square() {
    constexpr float kQuarter = 1.5707964F;
    return {
        .startRotation = FromAxisAngle({.z = 1.0F}, kQuarter),
        .loopRotation = FromAxisAngle({.z = 1.0F}, kQuarter),
        .loopPoint = 0,
        .points = {Turn(0.0F, 20.0F, kQuarter, 700), Turn(-20.0F, 0.0F, 2.0F * kQuarter, 650),
                   Turn(0.0F, -20.0F, 3.0F * kQuarter, 800), Turn(20.0F, 0.0F, 0.0F, 610)},
    };
}

class PaceCarScene final : public Scene {
public:
    void Tick([[maybe_unused]] const sim::InputFrame& input) override {
        car_.Tick();
        cars.push_back(car_.Car());
        cameras.push_back(car_.Camera());
    }

    void Snapshot(sim::RenderSnapshot& out) const override {
        out.entities.assign(1, {.id = sim::EntityId{1}, .transform = car_.Car()});
        out.camera = {.transform = car_.Camera()};
    }

    void Render(const sim::RenderSnapshot& interpolated) override {
        renderedCars.push_back(interpolated.entities.front().transform);
        renderedCameras.push_back(interpolated.camera.transform);
    }

    std::vector<Transform> cars;
    std::vector<Transform> cameras;
    std::vector<Transform> renderedCars;
    std::vector<Transform> renderedCameras;

private:
    PaceCar car_{Square()};
};

const PaceCarScene& Run(const std::vector<Nanoseconds>& frameTimes, FrameLoop& loop) {
    auto scene = std::make_unique<PaceCarScene>();
    const PaceCarScene& running = *scene;
    loop.Scenes().Push(std::move(scene));
    for (const Nanoseconds now : frameTimes) {
        static_cast<void>(loop.RunFrame(now));
    }
    return running;
}

TEST_CASE("the pace car takes the same poses per tick at any framerate") {
    const Nanoseconds duration = std::chrono::seconds{10};
    FrameLoop uncappedLoop;
    const PaceCarScene& uncapped = Run(test::UncappedFrames(duration), uncappedLoop);

    CHECK(uncapped.cars.size() == duration / kSimTick);
    for (const int fps : {30, 60, 144}) {
        CAPTURE(fps);
        FrameLoop loop;
        const PaceCarScene& paced = Run(test::FixedRateFrames(fps, duration), loop);
        CHECK(paced.cars == uncapped.cars);
        CHECK(paced.cameras == uncapped.cameras);
    }
}

std::vector<Transform> RenderedAt(const std::vector<Nanoseconds>& frameTimes,
                                  const std::vector<Nanoseconds>& sampleTimes) {
    FrameLoop loop;
    const PaceCarScene& scene = Run(frameTimes, loop);
    std::vector<Transform> poses;
    for (std::size_t i = 0; i < frameTimes.size(); ++i) {
        if (std::ranges::binary_search(sampleTimes, frameTimes[i])) {
            poses.push_back(scene.renderedCars[i]);
            poses.push_back(scene.renderedCameras[i]);
        }
    }
    return poses;
}

TEST_CASE("the pace car renders the same poses at any framerate when frames line up") {
    const Nanoseconds duration = std::chrono::seconds{10};
    const std::vector<Nanoseconds> shared = test::FixedRateFrames(6, duration);
    std::vector<Nanoseconds> uncappedFrames = test::UncappedFrames(duration);
    uncappedFrames.insert(uncappedFrames.end(), shared.begin(), shared.end());
    std::ranges::sort(uncappedFrames);
    const auto [last, end] = std::ranges::unique(uncappedFrames);
    uncappedFrames.erase(last, end);

    const std::vector<Transform> uncapped = RenderedAt(uncappedFrames, shared);

    CHECK(uncapped.size() == 2 * shared.size());
    for (const int fps : {30, 60, 144}) {
        CAPTURE(fps);
        CHECK(RenderedAt(test::FixedRateFrames(fps, duration), shared) == uncapped);
    }
}

TEST_CASE("the chase camera looks at the pace car from behind and above") {
    PaceCar car(Square());
    for (int tick = 0; tick < 40; ++tick) {
        car.Tick();
    }

    const Transform body = car.Car();
    const Transform camera = car.Camera();
    const Vector3 ahead = Apply({.rotation = body.rotation}, ToEngine({.y = 1.0F}));
    const Vector3 view = Apply({.rotation = camera.rotation}, {.z = -1.0F});

    CHECK(camera.position.y > body.position.y);
    CHECK((ahead.x * view.x) + (ahead.z * view.z) > 0.9F);
}

}
}
