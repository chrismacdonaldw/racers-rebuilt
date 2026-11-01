#include "sim/track/track_timers.hpp"

#include <cstdint>

namespace rr::sim {
namespace {

constexpr std::uint32_t kRandomTableSize = 1024;
constexpr std::int64_t kLargestModulus = kRandomTableSize - 1;

Milliseconds Length(const assets::TimerPhase& phase, Milliseconds length, Random& random) {
    return phase.isRandom ? DrawPhaseLength(length, random) : length;
}

Milliseconds LessOvershoot(Milliseconds length, Milliseconds fired, Milliseconds otherPhase) {
    if (fired < otherPhase) {
        return length;
    }
    const Milliseconds overshoot = fired - otherPhase;
    return overshoot < length ? length - overshoot : length;
}

}

Milliseconds DrawPhaseLength(Milliseconds maximum, Random& random) {
    const auto value = static_cast<std::int64_t>(random.Below(kRandomTableSize));
    const std::int64_t limit = maximum.count();
    if (limit <= 0) {
        return Milliseconds{0};
    }
    if (limit <= kLargestModulus) {
        return Milliseconds{value % limit};
    }
    return Milliseconds{value * (limit / kLargestModulus)};
}

TrackTimers::TrackTimers(std::span<const assets::TrackTimer> timers, Random& random) {
    timers_.reserve(timers.size());
    for (const assets::TrackTimer& timer : timers) {
        Running& running =
            timers_.emplace_back(Running{.timer = timer, .delayLeft = timer.startDelay});
        if (timer.startDelay.count() == 0) {
            running.interval = Length(timer.off, timer.off.length, random);
        }
    }
}

void TrackTimers::Tick(Milliseconds elapsed, Random& random, TrackEventQueue& queue) {
    for (Running& running : timers_) {
        if (running.interval) {
            running.total += elapsed;
            if (running.total >= *running.interval) {
                Fire(running, random, queue);
            }
        } else if (elapsed >= running.delayLeft) {
            running.delayLeft = Milliseconds{0};
            running.interval = Length(running.timer.on, running.timer.on.length, random);
        } else {
            running.delayLeft -= elapsed;
        }
    }
}

void TrackTimers::Fire(Running& running, Random& random, TrackEventQueue& queue) {
    const assets::TrackTimer& timer = running.timer;
    const Milliseconds fired = running.total;
    queue.Fire({.id = timer.event, .edge = running.isOn ? EventEdge::End : EventEdge::Start});
    const assets::TimerPhase& next = running.isOn ? timer.off : timer.on;
    const assets::TimerPhase& ended = running.isOn ? timer.on : timer.off;
    running.isOn = !running.isOn;
    running.total = Milliseconds{0};
    running.interval = Length(next, LessOvershoot(next.length, fired, ended.length), random);
}

std::optional<Milliseconds> TrackTimers::PhaseTime(std::size_t timer) const {
    const Running& running = timers_.at(timer);
    return running.interval ? std::optional(running.total) : std::nullopt;
}

}
