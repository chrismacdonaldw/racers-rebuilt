#pragma once

#include "core/duration.hpp"
#include "core/result.hpp"
#include "platform/win32/unique_handle.hpp"

namespace rr::platform {

class FrameTimer {
public:
    [[nodiscard]] static Result<FrameTimer> Create();

    /// `deadline` is on the `Now()` clock. Returns at once if it has passed.
    void SleepUntil(Nanoseconds deadline) const;

private:
    explicit FrameTimer(void* handle);

    UniqueHandle timer_;
};

}
