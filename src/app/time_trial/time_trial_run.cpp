#include "app/time_trial/time_trial_run.hpp"

#include <format>
#include <utility>

#include "core/log.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kRecordStartSlot = 2;
constexpr std::size_t kAloneStartSlot = 1;

std::optional<assets::GhostRun> LoadRecord(const assets::Vfs& vfs, std::string_view track) {
    const std::string path = std::format("GAMEDATA/{}/GHOST.GHB", track);
    Result<assets::GhostRun> record = vfs.Read(path).and_then(
        [](const assets::FileData& file) { return assets::ParseGhb(file.Bytes()); });
    if (!record) {
        if (record.error().code != ErrorCode::NotFound) {
            log::Warn("{}: {}, racing without a record ghost", path, record.error().message);
        }
        return std::nullopt;
    }
    return std::move(*record);
}

}

TimeTrialRun::TimeTrialRun(std::string track, std::optional<assets::GhostRun> record,
                           bool isMirrored)
    : track_(std::move(track)),
      isMirrored_(isMirrored),
      hasRecord_(record && !record->samples.empty()),
      ghosts_(std::move(record)) {}

TimeTrialRun TimeTrialRun::Load(const assets::Vfs& vfs, std::string track, bool isMirrored) {
    std::optional<assets::GhostRun> record = LoadRecord(vfs, track);
    if (record && isMirrored) {
        record = assets::Mirrored(std::move(*record));
    }
    return {std::move(track), std::move(record), isMirrored};
}

std::size_t TimeTrialRun::StartSlot() const {
    return hasRecord_ ? kRecordStartSlot : kAloneStartSlot;
}

void TimeTrialRun::Finish(const assets::GhostLapTimes& laps, const std::string& holder) {
    ghosts_.Finish(laps);
    FoldRun(times_, laps, holder);
}

TrialOutcome TimeTrialRun::Outcome() const {
    return {.track = track_, .times = times_, .hasBeatenRecord = ghosts_.HasBeatenRecord()};
}

}
