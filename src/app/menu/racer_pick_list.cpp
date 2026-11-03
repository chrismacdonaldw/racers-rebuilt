#include "app/menu/racer_pick_list.hpp"

#include <algorithm>
#include <iterator>

#include "app/racer/racer_ref.hpp"

namespace rr::app {

std::vector<settings::Racer> PickableRacers(const settings::RacerStore& store,
                                            std::span<const settings::Racer> defaults) {
    std::vector<settings::Racer> racers = store.racers;
    for (const settings::Racer& racer : defaults) {
        const RacerRef ref = RefOf(racer);
        if (std::ranges::none_of(store.racers,
                                 [&](const settings::Racer& held) { return IsRef(held, ref); })) {
            racers.push_back(racer);
        }
    }
    return racers;
}

void SelectPicked(settings::RacerStore& store, std::span<const settings::Racer> racers,
                  std::size_t picked) {
    if (picked >= racers.size()) {
        return;
    }
    const RacerRef ref = RefOf(racers[picked]);
    const auto held = std::ranges::find_if(
        store.racers, [&](const settings::Racer& racer) { return IsRef(racer, ref); });
    if (held == store.racers.end()) {
        store.racers.push_back(racers[picked]);
        store.selected = store.racers.size() - 1;
        return;
    }
    store.selected = static_cast<std::size_t>(std::distance(store.racers.begin(), held));
}

}
