#include "sim/race/standings.hpp"

#include <algorithm>
#include <numeric>

#include "core/assert.hpp"

namespace rr::sim {
namespace {

bool IsAhead(const RacerStanding& a, const RacerStanding& b) {
    if (a.finishPlace && b.finishPlace) {
        return *a.finishPlace < *b.finishPlace;
    }
    if (a.finishPlace || b.finishPlace) {
        return a.finishPlace.has_value();
    }
    if (a.progress != b.progress) {
        return a.progress > b.progress;
    }
    return a.distanceToNextGate < b.distanceToNextGate;
}

}

void RankRacers(std::span<const RacerStanding> racers, std::span<std::size_t> order) {
    RR_ASSERT(order.size() == racers.size());
    std::ranges::iota(order, std::size_t{0});
    std::ranges::sort(order, [&](std::size_t a, std::size_t b) {
        return IsAhead(racers[a], racers[b]) || (!IsAhead(racers[b], racers[a]) && a < b);
    });
}

}
