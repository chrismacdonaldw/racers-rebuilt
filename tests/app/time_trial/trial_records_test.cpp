#include "app/time_trial/trial_records.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>

#include "app/circuit_awards.hpp"
#include "assets/archive_survey.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

using settings::BestTime;
using settings::Progress;
using settings::TrackRecords;

constexpr std::int32_t kRacec0r0Index = 5;
constexpr std::array kRun{Milliseconds{31000}, Milliseconds{30000}, Milliseconds{32000}};
constexpr std::array kTiedLapRun{Milliseconds{30000}, Milliseconds{40000}, Milliseconds{40000}};

bool BeatsLast(Progress& progress, std::size_t races) {
    bool isLast = false;
    for (std::size_t race = 0; race < races; ++race) {
        isLast = BeatRecord(progress, race);
    }
    return isLast;
}

TrackRecords Times(std::int64_t lap, std::int64_t race) {
    return {.lap = BestTime{.ms = lap, .holder = "Basil"},
            .race = BestTime{.ms = race, .holder = "Basil"}};
}

TEST_CASE("a finished run keeps its fastest lap and total, and an equal time takes over") {
    TrackRecords session;

    FoldRun(session, kRun, "Basil");
    FoldRun(session, kTiedLapRun, "Rocket");

    CHECK(session == TrackRecords{.lap = BestTime{.ms = 30000, .holder = "Rocket"},
                                  .race = BestTime{.ms = 93000, .holder = "Basil"}});
}

TEST_CASE("saved times only give way to strictly lower ones") {
    TrackRecords saved = Times(30000, 95000);

    CHECK_FALSE(CommitTimes(saved, Times(30000, 95000)));
    CHECK(CommitTimes(saved, Times(30000, 94999)));
    CHECK(CommitTimes(saved, Times(29999, 99999)));

    CHECK(saved == Times(29999, 94999));
}

TEST_CASE("the twelfth beaten record unlocks part set 7, once") {
    Progress progress;

    CHECK_FALSE(BeatsLast(progress, settings::kRecordRaces - 1));
    CHECK(BeatRecord(progress, settings::kRecordRaces - 1));
    CHECK_FALSE(BeatRecord(progress, settings::kRecordRaces - 1));
    CHECK_FALSE(BeatRecord(progress, settings::kRecordRaces));

    CHECK(progress.partSets == std::set{kRecordsPartSet});
    CHECK(progress.beatenRecords.size() == settings::kRecordRaces);
}

TEST_CASE("beating RACEC0R0's record saves its race list bit and the session's times") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / kProgressFileName;
    const TrialOutcome outcome{
        .track = "RACEC0R0", .times = Times(30000, 95000), .hasBeatenRecord = true};

    CHECK(SaveTrial(file, *vfs, outcome) == false);

    CHECK(settings::LoadProgress(file) ==
          Progress{.beatenRecords = {kRacec0r0Index}, .bestTimes = {{"RACEC0R0", outcome.times}}});
}

TEST_CASE("a time trial that sets nothing doesn't write the file") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / kProgressFileName;

    CHECK(SaveTrial(file, *vfs, {.track = "RACEC0R0"}) == false);

    CHECK_FALSE(std::filesystem::exists(file));
}

}
}
