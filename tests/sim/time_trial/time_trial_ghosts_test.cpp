#include "sim/time_trial/time_trial_ghosts.hpp"

#include <doctest/doctest.h>

#include <array>
#include <optional>

#include "ghost_run_rig.hpp"

namespace rr::sim {
namespace {

using test::At;
using test::FrameOf;
using test::Laps;
using test::StraightGhost;

constexpr Milliseconds kTick{10};

void Drive(TimeTrialGhosts& ghosts, Milliseconds length) {
    for (Milliseconds time{0}; time < length; time += kTick) {
        ghosts.Tick(kTick, At(3.0F));
    }
}

void RunAndFinish(TimeTrialGhosts& ghosts, const assets::GhostLapTimes& laps) {
    ghosts.Start();
    Drive(ghosts, Milliseconds{1000});
    ghosts.Finish(laps);
    ghosts.Restart();
}

TEST_CASE("a record ghost waits at its start pose until the go, then follows its samples") {
    TimeTrialGhosts ghosts(StraightGhost(10, Laps(100, 100, 100)));

    CHECK(FrameOf(ghosts.RecordFrame()).pose.position == Vector3{});
    CHECK_FALSE(ghosts.BestFrame().has_value());

    ghosts.Start();
    Drive(ghosts, Milliseconds{750});

    CHECK(FrameOf(ghosts.RecordFrame()).pose.position == Vector3{.x = 3.0F});
}

TEST_CASE("a record without samples is no record") {
    TimeTrialGhosts ghosts(StraightGhost(0, Laps(100, 100, 100)));

    CHECK_FALSE(ghosts.RecordFrame().has_value());
    RunAndFinish(ghosts, Laps(1, 1, 1));
    CHECK_FALSE(ghosts.HasBeatenRecord());
}

TEST_CASE("a record ghost that runs out of samples hides until the restart") {
    TimeTrialGhosts ghosts(StraightGhost(3, Laps(100, 100, 100)));
    ghosts.Start();

    Drive(ghosts, Milliseconds{500});
    CHECK_FALSE(ghosts.RecordFrame().has_value());
    ghosts.Finish(Laps(1, 1, 1));
    CHECK_FALSE(ghosts.RecordFrame().has_value());

    ghosts.Restart();
    CHECK(ghosts.RecordFrame().has_value());
}

TEST_CASE("after the player finishes, a record ghost still racing returns to its start") {
    TimeTrialGhosts ghosts(StraightGhost(10, Laps(100, 100, 100)));
    ghosts.Start();
    Drive(ghosts, Milliseconds{500});

    ghosts.Finish(Laps(1, 1, 1));

    CHECK(FrameOf(ghosts.RecordFrame()).pose.position == Vector3{});
}

TEST_CASE("the best ghost shows from the go after a finished run and replays that run") {
    TimeTrialGhosts ghosts(std::nullopt);
    RunAndFinish(ghosts, Laps(100, 100, 100));

    CHECK_FALSE(ghosts.BestFrame().has_value());
    ghosts.Start();
    Drive(ghosts, Milliseconds{500});

    CHECK(FrameOf(ghosts.BestFrame()).pose.position == Vector3{.x = 3.0F});
    ghosts.Finish(Laps(100, 100, 100));
    CHECK_FALSE(ghosts.BestFrame().has_value());
}

TEST_CASE("only a strictly faster run replaces the best") {
    TimeTrialGhosts ghosts(StraightGhost(10, Laps(100, 100, 100)));
    RunAndFinish(ghosts, Laps(100, 100, 100));
    CHECK_FALSE(ghosts.HasBeatenRecord());

    RunAndFinish(ghosts, Laps(100, 100, 99));
    CHECK(ghosts.HasBeatenRecord());

    RunAndFinish(ghosts, Laps(200, 200, 200));
    CHECK(ghosts.HasBeatenRecord());
}

TEST_CASE("a restart drops the run in progress and keeps the best ghost") {
    TimeTrialGhosts ghosts(std::nullopt);
    RunAndFinish(ghosts, Laps(100, 100, 100));
    ghosts.Start();
    Drive(ghosts, Milliseconds{500});

    ghosts.Restart();

    CHECK(FrameOf(ghosts.BestFrame()).pose.position == At(3.0F).position);
}

TEST_CASE("ghosts aren't drawn from 240 s after the go") {
    TimeTrialGhosts ghosts(StraightGhost(1000, Laps(100, 100, 100)));
    ghosts.Start();

    Drive(ghosts, Milliseconds{239990});
    CHECK(ghosts.RecordFrame().has_value());
    Drive(ghosts, kTick);
    CHECK_FALSE(ghosts.RecordFrame().has_value());
}

TEST_CASE("the time behind compares completed laps with the faster of the record and best") {
    TimeTrialGhosts ghosts(StraightGhost(10, Laps(100, 100, 100)));
    const std::array player{Milliseconds{130}, Milliseconds{100}};

    CHECK(ghosts.TimeBehind(player) == Milliseconds{30});
    CHECK(ghosts.TimeBehind(std::array{Milliseconds{90}}) == Milliseconds{0});

    RunAndFinish(ghosts, Laps(80, 80, 80));
    ghosts.Start();
    CHECK(ghosts.TimeBehind(player) == Milliseconds{70});

    TimeTrialGhosts noRecord(std::nullopt);
    CHECK(noRecord.TimeBehind(player) == Milliseconds{0});
    RunAndFinish(noRecord, Laps(120, 120, 120));
    noRecord.Start();
    CHECK(noRecord.TimeBehind(player) == Milliseconds{0});
    CHECK(noRecord.TimeBehind(std::array{Milliseconds{150}}) == Milliseconds{30});
}

}
}
