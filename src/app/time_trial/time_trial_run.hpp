#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "app/time_trial/trial_records.hpp"
#include "assets/ghost_run.hpp"
#include "assets/vfs.hpp"
#include "settings/progress.hpp"
#include "sim/time_trial/time_trial_ghosts.hpp"

namespace rr::app {

/// Keeps the ghosts and best times from one race of a time trial to the next.
class TimeTrialRun {
public:
    TimeTrialRun(std::string track, std::optional<assets::GhostRun> record);

    /// A race without a readable `GHOST.GHB` has no record.
    [[nodiscard]] static TimeTrialRun Load(const assets::Vfs& vfs, std::string track);

    [[nodiscard]] const std::string& Track() const { return track_; }
    [[nodiscard]] std::size_t StartSlot() const;
    [[nodiscard]] sim::TimeTrialGhosts& Ghosts() { return ghosts_; }
    [[nodiscard]] const sim::TimeTrialGhosts& Ghosts() const { return ghosts_; }

    void Finish(const assets::GhostLapTimes& laps, const std::string& holder);

    [[nodiscard]] TrialOutcome Outcome() const;

private:
    std::string track_;
    bool hasRecord_ = false;
    sim::TimeTrialGhosts ghosts_;
    settings::TrackRecords times_;
};

}
