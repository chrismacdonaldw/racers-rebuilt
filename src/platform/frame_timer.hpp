#pragma once

#include <memory>

#include "core/duration.hpp"
#include "core/result.hpp"

namespace rr::platform {

class FrameTimer {
public:
    [[nodiscard]] static Result<FrameTimer> Create();

    /// `deadline` is on the `Now()` clock. Returns at once if it has passed.
    void SleepUntil(Nanoseconds deadline) const;

private:
    struct HandleCloser {
        void operator()(void* handle) const;
    };

    explicit FrameTimer(void* handle);

    std::unique_ptr<void, HandleCloser> timer_;
};

}
