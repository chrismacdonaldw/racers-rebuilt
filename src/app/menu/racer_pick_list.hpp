#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "settings/racers.hpp"

namespace rr::app {

/// `defaults` first, then the store's racers that aren't copies of them.
[[nodiscard]] std::vector<settings::Racer> PickableRacers(
    const settings::RacerStore& store, std::span<const settings::Racer> defaults);

[[nodiscard]] std::size_t SelectedPick(std::span<const settings::Racer> racers,
                                       const settings::RacerStore& store);

/// A default racer joins the store once, before it's selected.
void SelectPicked(settings::RacerStore& store, std::span<const settings::Racer> racers,
                  std::size_t picked);

}
