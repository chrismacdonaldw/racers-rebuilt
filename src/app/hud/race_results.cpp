#include "app/hud/race_results.hpp"

#include <algorithm>
#include <numeric>
#include <optional>
#include <span>

namespace rr::app {
namespace {

constexpr float kTop = 132.0F;
constexpr float kRowHeight = 32.0F;
constexpr std::size_t kMaxRows = 6;
constexpr float kRankX = 20.0F;
constexpr float kNameX = 50.0F;
constexpr float kStatusX = 500.0F;
constexpr float kBehindX = 520.0F;

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

void LayoutResults(std::span<const ResultRow> rows, std::span<const std::u16string> names,
                   const ResultLabels& labels, TextPool& pool, std::vector<HudText>& out) {
    for (std::size_t rank = 0; rank < std::min(rows.size(), kMaxRows); ++rank) {
        const ResultRow& row = rows[rank];
        const float y = kTop + (static_cast<float>(rank) * kRowHeight);
        out.push_back({.text = pool.Number(rank + 1), .x = kRankX, .y = y});
        if (row.racer < names.size()) {
            out.push_back({.text = names[row.racer], .x = kNameX, .y = y});
        }
        switch (row.status) {
            case ResultStatus::Winner:
                out.push_back({.text = labels.finished, .x = kStatusX, .y = y});
                break;
            case ResultStatus::Behind:
                out.push_back({.text = u"+", .x = kStatusX, .y = y});
                out.push_back({.text = pool.Time(row.behind), .x = kBehindX, .y = y});
                break;
            case ResultStatus::Racing:
                out.push_back({.text = labels.stillRacing, .x = kStatusX, .y = y});
                break;
        }
    }
}

}
