#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "app/scene_stack.hpp"
#include "core/duration.hpp"
#include "sim/input_latch.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

inline constexpr Milliseconds kSimTick{12};
inline constexpr Milliseconds kMaxFrameTime{250};
inline constexpr int kMaxStepsPerFrame = 8;

struct FrameStats {
    int tickCount = 0;
    float alpha = 0.0F;
};

/// Time beyond `kMaxStepsPerFrame` ticks in one frame is dropped, not simulated.
class FrameLoop {
public:
    /// `now` is monotonic. The first call only sets the time base.
    FrameStats RunFrame(Nanoseconds now);

    [[nodiscard]] SceneStack& Scenes() { return scenes_; }
    [[nodiscard]] sim::InputLatch& Input() { return input_; }

private:
    void RestartSnapshots(const Scene& scene);
    int RunTicks(Scene& scene);

    SceneStack scenes_;
    sim::InputLatch input_;
    std::optional<Nanoseconds> lastFrame_;
    Nanoseconds accumulator_{0};
    std::uint64_t tick_ = 0;
    sim::RenderSnapshot previous_;
    sim::RenderSnapshot current_;
    sim::RenderSnapshot interpolated_;
};

}
