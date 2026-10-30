#pragma once

#include <cstdint>

#include "core/duration.hpp"

namespace rr::sim {

enum class RacePhase : std::uint8_t { Intro, Countdown, Racing, Finished, Results };

class RaceClock {
public:
    /// Racing ends when `hasPlayerFinished`. Every other phase ends when its time is up.
    void Advance(Milliseconds elapsed, bool hasPlayerFinished);

    [[nodiscard]] RacePhase Phase() const { return phase_; }
    [[nodiscard]] Milliseconds PhaseTime() const { return phaseTime_; }

    /// Time spent racing and finished.
    [[nodiscard]] Milliseconds RaceTime() const { return raceTime_; }

    [[nodiscard]] bool IsRunning() const {
        return phase_ == RacePhase::Racing || phase_ == RacePhase::Finished;
    }

private:
    void Enter(RacePhase phase);

    RacePhase phase_ = RacePhase::Intro;
    Milliseconds phaseTime_{0};
    Milliseconds raceTime_{0};
};

}
