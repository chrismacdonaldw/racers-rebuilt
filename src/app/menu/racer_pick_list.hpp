#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "settings/racers.hpp"

namespace rr::app {

/// The store's racers, then each of `defaults` the store doesn't already hold.
[[nodiscard]] std::vector<settings::Racer> PickableRacers(
    const settings::RacerStore& store, std::span<const settings::Racer> defaults);

/// A default racer joins the store once, before it's selected.
void SelectPicked(settings::RacerStore& store, std::span<const settings::Racer> racers,
                  std::size_t picked);

}
