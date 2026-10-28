#include "platform/frame_timer.hpp"

#include <windows.h>

#include <chrono>
#include <format>

#include "core/log.hpp"
#include "platform/clock.hpp"

namespace rr::platform {
namespace {

constexpr Nanoseconds kSpinMargin = std::chrono::microseconds{500};
constexpr std::int64_t kNanosecondsPerTimerUnit = 100;

HANDLE CreateTimer() {
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS);
    if (timer != nullptr) {
        return timer;
    }
    log::Warn("high-resolution timer unavailable (error {}), using a standard waitable timer",
              GetLastError());
    return CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
}

}

Result<FrameTimer> FrameTimer::Create() {
    HANDLE timer = CreateTimer();
    if (timer == nullptr) {
        return Fail(ErrorCode::Platform,
                    std::format("can't create a frame timer (error {})", GetLastError()));
    }
    return FrameTimer(timer);
}

void FrameTimer::SleepUntil(Nanoseconds deadline) const {
    const Nanoseconds sleepTime = deadline - Now() - kSpinMargin;
    if (sleepTime > Nanoseconds{0}) {
        LARGE_INTEGER dueTime{};
        dueTime.QuadPart = -(sleepTime.count() / kNanosecondsPerTimerUnit);
        if (SetWaitableTimerEx(timer_.get(), &dueTime, 0, nullptr, nullptr, nullptr, 0) != FALSE) {
            WaitForSingleObject(timer_.get(), INFINITE);
        }
    }
    while (Now() < deadline) {
        YieldProcessor();
    }
}

FrameTimer::FrameTimer(void* handle) : timer_(handle) {}

void FrameTimer::HandleCloser::operator()(void* handle) const {
    CloseHandle(handle);
}

}
