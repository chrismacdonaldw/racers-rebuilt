#include "app/hud/race_results.hpp"

#include <numeric>
#include <optional>
#include <span>

namespace rr::app {
namespace {

std::optional<Milliseconds> RaceTotal(const sim::RaceProgress& progress, std::size_t racer) {
    const sim::RacerProgress& own = progress.OfRacer(racer);
    if (own.laps.CompletedLaps() < progress.LapCount()) {
        return std::nullopt;
    }
    return std::accumulate(own.lapTimes.begin(), own.lapTimes.end(), Milliseconds{0});
}

}

void ReadResults(const sim::RaceProgress& progress, std::vector<ResultRow>& out) {
    out.clear();
    const std::span<const std::size_t> standings = progress.Standings();
    if (standings.empty()) {
        return;
    }
    const std::optional<Milliseconds> winnerTotal = RaceTotal(progress, standings.front());
    for (const std::size_t racer : standings) {
        const std::optional<Milliseconds> total = RaceTotal(progress, racer);
        if (racer == standings.front()) {
            out.push_back({.racer = racer, .status = ResultStatus::Winner});
        } else if (total && winnerTotal) {
            out.push_back(
                {.racer = racer, .status = ResultStatus::Behind, .behind = *total - *winnerTotal});
        } else {
            out.push_back({.racer = racer});
        }
    }
}

}
