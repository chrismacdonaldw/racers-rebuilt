#include "app/frame_loop.hpp"

#include <algorithm>
#include <utility>

#include "sim/interpolation.hpp"

namespace rr::app {

FrameStats FrameLoop::RunFrame(Nanoseconds now) {
    const bool isNewScene = scenes_.ApplyPendingChanges();
    const Nanoseconds elapsed =
        lastFrame_ ? std::min<Nanoseconds>(now - *lastFrame_, kMaxFrameTime) : Nanoseconds{0};
    lastFrame_ = now;

    Scene* scene = scenes_.Top();
    if (scene == nullptr) {
        return {};
    }
    if (isNewScene) {
        RestartSnapshots(*scene);
    }

    accumulator_ += elapsed;
    const int tickCount = RunTicks(*scene);
    const float alpha = static_cast<float>(accumulator_.count()) /
                        static_cast<float>(Nanoseconds{kSimTick}.count());
    sim::Interpolate(previous_, current_, alpha, interpolated_);
    scene->Render(interpolated_);
    return {.tickCount = tickCount, .alpha = alpha, .isNewScene = isNewScene};
}

void FrameLoop::RestartSnapshots(const Scene& scene) {
    tick_ = 0;
    accumulator_ = Nanoseconds{0};
    scene.Snapshot(current_);
    current_.tick = tick_;
    previous_ = current_;
}

int FrameLoop::RunTicks(Scene& scene) {
    int tickCount = 0;
    while (accumulator_ >= kSimTick && tickCount < kMaxStepsPerFrame) {
        std::swap(previous_, current_);
        scene.Tick(input_.TakeFrame());
        scene.Snapshot(current_);
        current_.tick = ++tick_;
        accumulator_ -= kSimTick;
        ++tickCount;
    }
    accumulator_ %= kSimTick;
    return tickCount;
}

}
