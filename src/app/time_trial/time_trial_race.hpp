#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "app/hud/race_hud.hpp"
#include "app/menu/pause_menu.hpp"
#include "app/time_trial/time_trial_run.hpp"
#include "sim/input_frame.hpp"
#include "sim/race/race_clock.hpp"
#include "sim/race/race_session.hpp"
#include "sim/time_trial/time_trial_ghosts.hpp"

namespace rr::app {

enum class TrialChoice : std::uint8_t { RaceAgain, Leave };

/// Records one race for the ghosts and asks RACE AGAIN 9.5 s after the player finishes.
class TimeTrialRace {
public:
    /// Sends the ghosts back to their start. `run` must outlive the race.
    explicit TimeTrialRace(TimeTrialRun& run);

    /// Call after each race tick. `holder` names the player in the best times.
    void Follow(const sim::RaceSession& session, std::u16string_view holder);

    /// While true the race waits for `Ask`.
    [[nodiscard]] bool IsAsking(const sim::RaceClock& clock) const;

    /// Returns the choice on the tick it's confirmed.
    [[nodiscard]] std::optional<TrialChoice> Ask(const sim::InputFrame& input);

    [[nodiscard]] PauseView Question() const;
    [[nodiscard]] const sim::TimeTrialGhosts& Ghosts() const { return run_->Ghosts(); }

private:
    TimeTrialRun* run_;
    bool isStarted_ = false;
    bool isFinished_ = false;
    ConfirmRow focus_ = ConfirmRow::Yes;
};

}
