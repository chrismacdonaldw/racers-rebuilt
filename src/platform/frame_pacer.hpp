#pragma once

#include <cstdint>
#include <optional>

#include "core/duration.hpp"
#include "platform/frame_timer.hpp"

namespace rr::platform {

enum class PresentMode : std::uint8_t {
    Vsync,
    Capped,
    Uncapped,
};

inline constexpr int kMinFpsCap = 30;
inline constexpr int kMaxFpsCap = 360;

struct PacingConfig {
    PresentMode mode = PresentMode::Vsync;
    int fpsCap = 144;
};

class FramePacer {
public:
    FramePacer(FrameTimer timer, PacingConfig config);

    void SetConfig(PacingConfig config);

    /// Only `Capped` waits here. In `Vsync` mode the swapchain present does the waiting.
    void WaitForNextFrame();

private:
    FrameTimer timer_;
    PacingConfig config_;
    std::optional<Nanoseconds> lastSlot_;
};

}
