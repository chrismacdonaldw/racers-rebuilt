#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "settings/progress.hpp"

namespace rr::app {

inline constexpr std::int32_t kRecordsPartSet = 7;

struct TrialOutcome {
    std::string track;
    settings::TrackRecords times;
    bool hasBeatenRecord = false;
};

/// A finished run's fastest lap and race time each replace the session's when lower or equal.
void FoldRun(settings::TrackRecords& session, std::span<const Milliseconds> laps,
             const std::string& holder);

/// Saves each session time that strictly beats the saved one. Returns whether any did.
bool CommitTimes(settings::TrackRecords& saved, const settings::TrackRecords& session);

/// Returns whether `race` was the last of the 12 records, which also unlocks part set 7.
bool BeatRecord(settings::Progress& progress, std::size_t race);

/// Returns whether the last record fell. Writes only on a change, and never over a bad file.
[[nodiscard]] Result<bool> SaveTrial(const std::filesystem::path& file, const assets::Vfs& vfs,
                                     const TrialOutcome& outcome);

}
