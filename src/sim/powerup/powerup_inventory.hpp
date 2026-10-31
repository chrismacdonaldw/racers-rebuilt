#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>

#include "assets/brick_layout.hpp"

namespace rr::sim {

inline constexpr std::size_t kMaxWhiteBricks = 3;

/// White bricks are indexes into the race's `BrickField`.
class PowerupInventory {
public:
    [[nodiscard]] std::optional<assets::BrickColour> Colour() const { return colour_; }
    [[nodiscard]] std::size_t WhiteCount() const;

    /// In pickup slot order, with gaps where a brick was lost.
    [[nodiscard]] std::span<const std::optional<std::size_t>> WhiteBricks() const {
        return whites_;
    }

    /// Returns the colour held before, which the brick takes in a swap.
    std::optional<assets::BrickColour> TakeColour(assets::BrickColour colour);

    /// Fails when already holding `kMaxWhiteBricks`.
    [[nodiscard]] bool TakeWhite(std::size_t brick);

    [[nodiscard]] std::optional<std::size_t> LoseWhite();

    [[nodiscard]] std::array<std::optional<std::size_t>, kMaxWhiteBricks> Spend();

    friend bool operator==(const PowerupInventory&, const PowerupInventory&) = default;

private:
    std::optional<assets::BrickColour> colour_;
    std::array<std::optional<std::size_t>, kMaxWhiteBricks> whites_{};
};

}
