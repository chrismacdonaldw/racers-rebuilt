#include "sim/powerup/powerup_inventory.hpp"

#include <algorithm>
#include <utility>

namespace rr::sim {
namespace {

bool IsHeld(const std::optional<std::size_t>& white) {
    return white.has_value();
}

}

std::size_t PowerupInventory::WhiteCount() const {
    return static_cast<std::size_t>(std::ranges::count_if(whites_, IsHeld));
}

std::optional<assets::BrickColour> PowerupInventory::TakeColour(assets::BrickColour colour) {
    return std::exchange(colour_, colour);
}

bool PowerupInventory::TakeWhite(std::size_t brick) {
    const auto slot = std::ranges::find_if_not(whites_, IsHeld);
    if (slot == whites_.end()) {
        return false;
    }
    *slot = brick;
    return true;
}

std::optional<std::size_t> PowerupInventory::LoseWhite() {
    const auto slot = std::ranges::find_if(whites_, IsHeld);
    if (slot == whites_.end()) {
        return std::nullopt;
    }
    return std::exchange(*slot, std::nullopt);
}

std::array<std::optional<std::size_t>, kMaxWhiteBricks> PowerupInventory::Spend() {
    colour_.reset();
    return std::exchange(whites_, {});
}

}
