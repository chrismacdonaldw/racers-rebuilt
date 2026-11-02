#include "app/time_trial/time_trial_race.hpp"

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <string_view>

#include "app/frame_loop.hpp"
#include "app/race_step.hpp"
#include "assets/ghost_run.hpp"
#include "core/utf16_text.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kQuestionDelay{9500};

assets::GhostLapTimes LapTimes(const sim::RacerProgress& progress) {
    assets::GhostLapTimes laps{};
    std::ranges::copy(
        progress.lapTimes | std::views::take(static_cast<std::ptrdiff_t>(laps.size())),
        laps.begin());
    return laps;
}

}

TimeTrialRace::TimeTrialRace(TimeTrialRun& run) : run_(&run) {
    run_->Ghosts().Restart();
}

void TimeTrialRace::Follow(const sim::RaceSession& session, std::u16string_view holder) {
    if (isFinished_) {
        return;
    }
    if (!isStarted_) {
        if (!session.Clock().IsRunning()) {
            return;
        }
        run_->Ghosts().Start();
        isStarted_ = true;
    }
    const sim::RacerProgress& player = session.Progress().OfRacer(kPlayerSlot);
    if (player.finishPlace) {
        run_->Finish(LapTimes(player), Utf8FromUtf16(holder));
        isFinished_ = true;
        return;
    }
    run_->Ghosts().Tick(kSimTick, session.Racers()[kPlayerSlot].Pose());
}

bool TimeTrialRace::IsAsking(const sim::RaceClock& clock) const {
    return isFinished_ && clock.Phase() == sim::RacePhase::Finished &&
           clock.PhaseTime() > kQuestionDelay;
}

std::optional<TrialChoice> TimeTrialRace::Ask(const sim::InputFrame& input) {
    if (input.WasPressed(sim::MenuAction::Up) != input.WasPressed(sim::MenuAction::Down)) {
        focus_ = focus_ == ConfirmRow::Yes ? ConfirmRow::No : ConfirmRow::Yes;
    }
    if (!input.WasPressed(sim::MenuAction::Confirm)) {
        return std::nullopt;
    }
    return focus_ == ConfirmRow::Yes ? TrialChoice::RaceAgain : TrialChoice::Leave;
}

PauseView TimeTrialRace::Question() const {
    return {.dialog = run_->Ghosts().HasBeatenRecord() ? PauseDialog::RecordBeaten
                                                       : PauseDialog::RecordStands,
            .focus = static_cast<std::size_t>(focus_)};
}

}
