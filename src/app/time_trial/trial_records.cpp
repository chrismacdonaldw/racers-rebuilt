#include "app/time_trial/trial_records.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/race_catalog.hpp"

namespace rr::app {
namespace {

void FoldTime(std::optional<settings::BestTime>& held, Milliseconds time,
              const std::string& holder) {
    if (!held || time.count() <= held->ms) {
        held = settings::BestTime{.ms = time.count(), .holder = holder};
    }
}

bool CommitTime(std::optional<settings::BestTime>& saved,
                const std::optional<settings::BestTime>& session) {
    if (!session || (saved && saved->ms <= session->ms)) {
        return false;
    }
    saved = session;
    return true;
}

std::optional<std::size_t> RecordRace(const assets::RaceCatalog& catalog, std::string_view track) {
    const std::span<const assets::RaceEntry> races = catalog.Races();
    const auto race = std::ranges::find_if(races, [&](const assets::RaceEntry& entry) {
        return !entry.isMirrored && assets::ToUpperAscii(entry.folder) == track;
    });
    if (race == races.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(race - races.begin());
}

}

void FoldRun(settings::TrackRecords& session, std::span<const Milliseconds> laps,
             const std::string& holder) {
    if (laps.empty()) {
        return;
    }
    FoldTime(session.lap, std::ranges::min(laps), holder);
    FoldTime(session.race, std::accumulate(laps.begin(), laps.end(), Milliseconds{0}), holder);
}

bool CommitTimes(settings::TrackRecords& saved, const settings::TrackRecords& session) {
    const bool lap = CommitTime(saved.lap, session.lap);
    return CommitTime(saved.race, session.race) || lap;
}

bool BeatRecord(settings::Progress& progress, std::size_t race) {
    if (race >= settings::kRecordRaces ||
        !progress.beatenRecords.insert(static_cast<std::int32_t>(race)).second) {
        return false;
    }
    const bool isLast = settings::HasBeatenEveryRecord(progress);
    if (isLast) {
        progress.partSets.insert(kRecordsPartSet);
    }
    return isLast;
}

Result<bool> SaveTrial(const std::filesystem::path& file, const assets::Vfs& vfs,
                       const TrialOutcome& outcome) {
    Result<assets::RaceCatalog> catalog = assets::RaceCatalog::Load(vfs);
    Result<settings::Progress> progress =
        catalog.and_then([&](const assets::RaceCatalog&) { return settings::LoadProgress(file); });
    if (!progress) {
        return std::unexpected(std::move(progress.error()));
    }
    const std::size_t before = progress->beatenRecords.size();
    const std::optional<std::size_t> race = RecordRace(*catalog, outcome.track);
    const bool unlocks = outcome.hasBeatenRecord && race && BeatRecord(*progress, *race);
    const bool hasNewTimes = CommitTimes(progress->bestTimes[outcome.track], outcome.times);
    if (!hasNewTimes && progress->beatenRecords.size() == before) {
        return false;
    }
    return settings::SaveProgress(*progress, file).transform([&] { return unlocks; });
}

}
