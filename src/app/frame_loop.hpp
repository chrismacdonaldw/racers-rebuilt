#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "app/scene_stack.hpp"
#include "core/duration.hpp"
#include "mods/hook_registry.hpp"
#include "sim/input_latch.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

inline constexpr Milliseconds kSimTick{12};
inline constexpr Milliseconds kMaxFrameTime{250};
inline constexpr int kMaxStepsPerFrame = 8;

/// Sees every tick of the running scene, so it can replace the input and inspect the result.
class TickTap {
public:
    virtual ~TickTap() = default;

    virtual void SceneStarted() = 0;
    virtual void BeforeTick(sim::InputFrame& input) = 0;
    virtual void AfterTick(const sim::InputFrame& input, const sim::RenderSnapshot& after) = 0;
};

struct FrameStats {
    int tickCount = 0;
    float alpha = 0.0F;
    bool isNewScene = false;
};

/// Time beyond `kMaxStepsPerFrame` ticks in one frame is dropped, not simulated. Emits the scene,
/// tick and frame hooks.
class FrameLoop {
public:
    /// `now` is monotonic. The first call only sets the time base.
    FrameStats RunFrame(Nanoseconds now);

    [[nodiscard]] SceneStack& Scenes() { return scenes_; }
    [[nodiscard]] sim::InputLatch& Input() { return input_; }
    [[nodiscard]] mods::HookRegistry& Hooks() { return hooks_; }

    /// `tap` must outlive the loop or be replaced. Null removes it.
    void SetTickTap(TickTap* tap) { tap_ = tap; }

private:
    void RestartSnapshots(const Scene& scene);
    int RunTicks(Scene& scene);

    SceneStack scenes_;
    mods::HookRegistry hooks_;
    sim::InputLatch input_;
    TickTap* tap_ = nullptr;
    std::optional<Nanoseconds> lastFrame_;
    Nanoseconds accumulator_{0};
    std::uint64_t tick_ = 0;
    sim::RenderSnapshot previous_;
    sim::RenderSnapshot current_;
    sim::RenderSnapshot interpolated_;
};

}
