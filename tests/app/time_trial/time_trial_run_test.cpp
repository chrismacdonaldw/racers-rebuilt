#include "app/time_trial/time_trial_run.hpp"

#include <doctest/doctest.h>

#include <optional>

#include "../../sim/time_trial/ghost_run_rig.hpp"
#include "app/time_trial/time_trial_race.hpp"
#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

using settings::BestTime;
using settings::TrackRecords;

constexpr Milliseconds kTick{12};

sim::InputFrame Pressing(sim::MenuAction action) {
    sim::InputFrame frame;
    frame.menuPressed.set(sim::InputFrame::Index(action));
    return frame;
}

TEST_CASE("a time trial starts the player on position 2 behind a record and 1 alone") {
    CHECK(TimeTrialRun("RACEC0R0", test::StraightGhost(4, test::Laps(1, 1, 1))).StartSlot() == 2);
    CHECK(TimeTrialRun("RACEC0R0", std::nullopt).StartSlot() == 1);
    CHECK(TimeTrialRun("RACEC0R0", assets::GhostRun{}).StartSlot() == 1);
}

TEST_CASE("a run under the record's total beats it and folds into the session's times") {
    TimeTrialRun run("RACEC0R0", test::StraightGhost(4, test::Laps(30000, 30000, 30000)));
    run.Ghosts().Start();
    run.Ghosts().Tick(kTick, test::At(0.0F));

    run.Finish(test::Laps(29000, 31000, 29999), "Basil");

    const TrialOutcome outcome = run.Outcome();
    CHECK(outcome.track == "RACEC0R0");
    CHECK(outcome.hasBeatenRecord);
    CHECK(outcome.times == TrackRecords{.lap = BestTime{.ms = 29000, .holder = "Basil"},
                                        .race = BestTime{.ms = 89999, .holder = "Basil"}});
}

TEST_CASE("RACE AGAIN opens on YES and says once the record has fallen") {
    TimeTrialRun run("RACEC0R0", test::StraightGhost(4, test::Laps(30000, 30000, 30000)));
    TimeTrialRace race(run);
    const PauseView stands = race.Question();
    run.Ghosts().Start();
    run.Finish(test::Laps(1000, 1000, 1000), "Basil");

    CHECK(stands.dialog == PauseDialog::RecordStands);
    CHECK(stands.focus == 0);
    CHECK(race.Question().dialog == PauseDialog::RecordBeaten);
    CHECK(race.Ask(Pressing(sim::MenuAction::Confirm)) == TrialChoice::RaceAgain);
}

TEST_CASE("NO on RACE AGAIN leaves the time trial") {
    TimeTrialRun run("RACEC0R0", std::nullopt);
    TimeTrialRace race(run);

    CHECK_FALSE(race.Ask(Pressing(sim::MenuAction::Down)).has_value());

    CHECK(race.Question().focus == 1);
    CHECK(race.Ask(Pressing(sim::MenuAction::Confirm)) == TrialChoice::Leave);
}

TEST_CASE("only retail races with a record ghost start the player on position 2") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    CHECK(TimeTrialRun::Load(*vfs, "RACEC0R0").StartSlot() == 2);
    CHECK(TimeTrialRun::Load(*vfs, "RACEC3R0").StartSlot() == 1);
}

}
}
