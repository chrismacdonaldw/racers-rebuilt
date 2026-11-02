#include "sim/race/circuit_run.hpp"

#include <algorithm>

#include "core/assert.hpp"

namespace rr::sim {
namespace {

constexpr std::array<std::int32_t, assets::kGridSlots> kPlacePoints{30, 20, 10, 3, 2, 1};
constexpr std::int32_t kKnockOutPointsPerRace = 10;
constexpr std::array kAwardsByRank{CircuitAward::Gold, CircuitAward::Silver, CircuitAward::Bronze};

}

std::int32_t PlacePoints(std::optional<int> place) {
    if (!place || *place < 1 || static_cast<std::size_t>(*place) > kPlacePoints.size()) {
        return 0;
    }
    return kPlacePoints[static_cast<std::size_t>(*place) - 1];
}

CircuitRun::CircuitRun(std::array<bool, assets::kCircuitRaces> hasRace, std::size_t player,
                       bool isSoloPlayer, const CircuitStanding& from)
    : hasRace_(hasRace), player_(player), isSoloPlayer_(isSoloPlayer), points_(from.points) {
    RR_VERIFY(player_ < points_.size());
    RR_VERIFY(std::ranges::contains(hasRace_, true));
    Enter(from.race);
}

CircuitStanding CircuitRun::Standing() const {
    return {.race = race_.value_or(hasRace_.size()), .points = points_};
}

std::size_t CircuitRun::Rank(std::size_t slot) const {
    return static_cast<std::size_t>(std::ranges::count_if(
        points_, [&](std::int32_t points) { return points > points_.at(slot); }));
}

void CircuitRun::FinishRace(std::span<const std::optional<int>> places) {
    if (!race_) {
        return;
    }
    for (std::size_t slot = 0; slot < std::min(places.size(), points_.size()); ++slot) {
        points_[slot] += PlacePoints(places[slot]);
    }
    Enter(*race_ + 1);
}

void CircuitRun::Restart() {
    points_.fill(0);
    award_.reset();
    Enter(0);
}

void CircuitRun::Quit() {
    race_.reset();
    award_.reset();
}

void CircuitRun::Enter(std::size_t race) {
    while (race < hasRace_.size() && !hasRace_[race]) {
        ++race;
    }
    if (race >= hasRace_.size()) {
        const std::size_t rank = Rank(player_);
        race_.reset();
        award_ = rank < kAwardsByRank.size() ? kAwardsByRank[rank] : CircuitAward::None;
        return;
    }
    if (isSoloPlayer_ &&
        points_[player_] < kKnockOutPointsPerRace * static_cast<std::int32_t>(race)) {
        race_.reset();
        award_ = CircuitAward::None;
        return;
    }
    race_ = race;
}

}
