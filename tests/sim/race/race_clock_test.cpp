#include "sim/race/race_clock.hpp"

#include <doctest/doctest.h>

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};

Milliseconds RunUntilPhaseChanges(RaceClock& clock, bool hasPlayerFinished = false) {
    const RacePhase start = clock.Phase();
    Milliseconds elapsed{0};
    for (int tick = 0; tick < 10000 && clock.Phase() == start; ++tick) {
        clock.Advance(kTick, hasPlayerFinished);
        elapsed += kTick;
    }
    return elapsed;
}

TEST_CASE("a race runs intro, countdown, racing, finished and results in order") {
    RaceClock clock;
    CHECK(clock.Phase() == RacePhase::Intro);

    CHECK(RunUntilPhaseChanges(clock) == Milliseconds{2004});
    CHECK(clock.Phase() == RacePhase::Countdown);
    CHECK(clock.PhaseTime() == Milliseconds{0});

    CHECK(RunUntilPhaseChanges(clock) == Milliseconds{3000});
    CHECK(clock.Phase() == RacePhase::Racing);

    CHECK(RunUntilPhaseChanges(clock, true) == kTick);
    CHECK(clock.Phase() == RacePhase::Finished);

    CHECK(RunUntilPhaseChanges(clock) == Milliseconds{10008});
    CHECK(clock.Phase() == RacePhase::Results);
}

TEST_CASE("racing lasts until a player finishes") {
    RaceClock clock;
    RunUntilPhaseChanges(clock);
    RunUntilPhaseChanges(clock);

    for (int tick = 0; tick < 50000; ++tick) {
        clock.Advance(kTick, false);
    }

    CHECK(clock.Phase() == RacePhase::Racing);
}

TEST_CASE("race time runs only while racing and finished") {
    RaceClock clock;
    RunUntilPhaseChanges(clock);
    RunUntilPhaseChanges(clock);
    CHECK(clock.RaceTime() == Milliseconds{0});

    clock.Advance(kTick, false);
    clock.Advance(kTick, true);
    RunUntilPhaseChanges(clock);
    const Milliseconds atResults = clock.RaceTime();
    clock.Advance(kTick, false);

    CHECK(atResults == Milliseconds{24 + 10008});
    CHECK(clock.RaceTime() == atResults);
}

TEST_CASE("results is the last phase") {
    RaceClock clock;
    RunUntilPhaseChanges(clock);
    RunUntilPhaseChanges(clock);
    RunUntilPhaseChanges(clock, true);
    RunUntilPhaseChanges(clock);

    RunUntilPhaseChanges(clock);

    CHECK(clock.Phase() == RacePhase::Results);
}

}
}
