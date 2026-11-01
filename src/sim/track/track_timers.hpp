#pragma once

#include <optional>
#include <span>
#include <vector>

#include "assets/track_triggers.hpp"
#include "core/duration.hpp"
#include "sim/race/random.hpp"
#include "sim/track/track_event.hpp"

namespace rr::sim {

/// A value below 1,024, then `value % maximum` up to 1,023 ms or `value × (maximum / 1023)` above.
[[nodiscard]] Milliseconds DrawPhaseLength(Milliseconds maximum, Random& random);

/// Starts each timer's event as its on phase begins and ends it as its off phase begins.
class TrackTimers {
public:
    TrackTimers(std::span<const assets::TrackTimer> timers, Random& random);

    void Tick(Milliseconds elapsed, Random& random, TrackEventQueue& queue);

    [[nodiscard]] std::size_t Count() const { return timers_.size(); }
    [[nodiscard]] bool IsOn(std::size_t timer) const { return timers_.at(timer).isOn; }

    /// Time since the running phase was scheduled, or none during the start delay.
    [[nodiscard]] std::optional<Milliseconds> PhaseTime(std::size_t timer) const;

private:
    struct Running {
        assets::TrackTimer timer;
        Milliseconds delayLeft{0};
        std::optional<Milliseconds> interval;
        Milliseconds total{0};
        bool isOn = false;
    };

    static void Fire(Running& running, Random& random, TrackEventQueue& queue);

    std::vector<Running> timers_;
};

}
