#include "sim/race/race_clock.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kIntroTime{2000};
constexpr Milliseconds kFinishedTime{9999};

}

void RaceClock::Advance(Milliseconds elapsed, bool hasPlayerFinished) {
    if (IsRunning()) {
        raceTime_ += elapsed;
    }
    phaseTime_ += elapsed;
    switch (phase_) {
        case RacePhase::Intro:
            if (phaseTime_ > kIntroTime) {
                Enter(RacePhase::Countdown);
            }
            break;
        case RacePhase::Countdown:
            if (phaseTime_ >= kCountdownLength) {
                Enter(RacePhase::Racing);
            }
            break;
        case RacePhase::Racing:
            if (hasPlayerFinished) {
                Enter(RacePhase::Finished);
            }
            break;
        case RacePhase::Finished:
            if (phaseTime_ > kFinishedTime) {
                Enter(RacePhase::Results);
            }
            break;
        case RacePhase::Results:
            break;
    }
}

void RaceClock::Enter(RacePhase phase) {
    phase_ = phase;
    phaseTime_ = Milliseconds{0};
}

}
