#include "app/menu/racer_pick_list.hpp"

#include <algorithm>
#include <iterator>

#include "app/racer/racer_ref.hpp"

namespace rr::app {

std::vector<settings::Racer> PickableRacers(const settings::RacerStore& store,
                                            std::span<const settings::Racer> defaults) {
    std::vector<settings::Racer> racers(defaults.begin(), defaults.end());
    for (const settings::Racer& racer : store.racers) {
        const RacerRef ref = RefOf(racer);
        if (std::ranges::none_of(
                defaults, [&](const settings::Racer& shipped) { return IsRef(shipped, ref); })) {
            racers.push_back(racer);
        }
    }
    return racers;
}

std::size_t SelectedPick(std::span<const settings::Racer> racers,
                         const settings::RacerStore& store) {
    if (!store.selected || *store.selected >= store.racers.size()) {
        return 0;
    }
    const RacerRef ref = RefOf(store.racers.at(*store.selected));
    const auto found = std::ranges::find_if(
        racers, [&](const settings::Racer& racer) { return IsRef(racer, ref); });
    return found == racers.end() ? 0 : static_cast<std::size_t>(found - racers.begin());
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
