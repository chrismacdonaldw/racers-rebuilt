#include "app/race_status.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <span>

namespace rr::app {
namespace {

constexpr std::int64_t kMsPerSecond = 1000;
constexpr std::int64_t kMsPerMinute = 60 * kMsPerSecond;

int Place(const sim::RaceProgress& progress, std::size_t racer) {
    if (const auto place = progress.OfRacer(racer).finishPlace) {
        return *place;
    }
    const std::span<const std::size_t> standings = progress.Standings();
    return static_cast<int>(std::ranges::find(standings, racer) - standings.begin()) + 1;
}

void AppendTime(Milliseconds time, std::string& out) {
    const std::int64_t ms = time.count();
    std::format_to(std::back_inserter(out), "{}:{:02}.{:03}", ms / kMsPerMinute,
                   (ms % kMsPerMinute) / kMsPerSecond, ms % kMsPerSecond);
}

}

void FormatRaceStatus(const sim::RaceSession& session, std::size_t racer, std::string& out) {
    out.clear();
    const sim::RaceClock& clock = session.Clock();
    const auto into = std::back_inserter(out);
    switch (clock.Phase()) {
        case sim::RacePhase::Intro:
            out = "Intro";
            return;
        case sim::RacePhase::Countdown: {
            const std::int64_t left = (sim::kCountdownLength - clock.PhaseTime()).count();
            std::format_to(into, "Countdown {}", (left + kMsPerSecond - 1) / kMsPerSecond);
            return;
        }
        case sim::RacePhase::Racing:
        case sim::RacePhase::Finished:
        case sim::RacePhase::Results:
            break;
    }
    const sim::RaceProgress& progress = session.Progress();
    const sim::RacerProgress& own = progress.OfRacer(racer);
    const std::size_t racerCount = session.Racers().size();
    if (own.finishPlace) {
        std::format_to(into, "Finished {} of {}  ", *own.finishPlace, racerCount);
    } else {
        const int lap = std::min(own.laps.CompletedLaps() + 1, progress.LapCount());
        std::format_to(into, "Lap {}/{}  Place {} of {}  ", lap, progress.LapCount(),
                       Place(progress, racer), racerCount);
    }
    AppendTime(clock.RaceTime(), out);
    if (own.laps.IsWrongWay()) {
        out += "  Wrong way";
    }
}

}
