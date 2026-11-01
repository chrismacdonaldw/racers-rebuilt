#include "sim/powerup/powerup_inventory.hpp"

#include <algorithm>
#include <utility>

#include "core/assert.hpp"

namespace rr::sim {
namespace {

bool IsHeld(const std::optional<std::size_t>& white) {
    return white.has_value();
}

}

std::size_t PowerupInventory::WhiteCount() const {
    return static_cast<std::size_t>(std::ranges::count_if(whites_, IsHeld)) + grantedWhites_;
}

std::optional<assets::BrickColour> PowerupInventory::TakeColour(assets::BrickColour colour) {
    return std::exchange(colour_, colour);
}

bool PowerupInventory::TakeWhite(std::size_t brick) {
    if (WhiteCount() == kMaxWhiteBricks) {
        return false;
    }
    *std::ranges::find_if_not(whites_, IsHeld) = brick;
    return true;
}

void PowerupInventory::Grant(assets::BrickColour colour, std::size_t level) {
    RR_ASSERT(*this == PowerupInventory{} && level <= kMaxWhiteBricks);
    colour_ = colour;
    grantedWhites_ = level;
}

std::optional<std::size_t> PowerupInventory::LoseWhite() {
    const auto slot = std::ranges::find_if(whites_, IsHeld);
    if (slot == whites_.end()) {
        grantedWhites_ -= std::min<std::size_t>(grantedWhites_, 1);
        return std::nullopt;
    }
    return std::exchange(*slot, std::nullopt);
}

std::array<std::optional<std::size_t>, kMaxWhiteBricks> PowerupInventory::Spend() {
    colour_.reset();
    grantedWhites_ = 0;
    return std::exchange(whites_, {});
}

}
