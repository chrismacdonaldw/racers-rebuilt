#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

#include "assets/piece_library.hpp"
#include "sim/car_build/built_car.hpp"
#include "sim/car_build/car_grid.hpp"

namespace rr::sim {

inline constexpr assets::CellSize kGroundSize{.width = 32, .height = 32};
inline constexpr PiecePlacement kCarOnGround{.x = 11, .y = 13};
inline constexpr std::size_t kMaxLoosePieces = 128;

/// Pieces lying on the ground around the car, off the cells under the car's grid. Taking one away
/// lets the pieces stacked on it settle.
class LooseBricks {
public:
    LooseBricks() = default;

    /// Lays `pieces` down again in order.
    explicit LooseBricks(std::span<const CarPiece> pieces);

    /// The height in plates the piece would rest at.
    [[nodiscard]] std::expected<int, PlacementFailure> Test(const assets::PieceRecord& piece,
                                                            PiecePlacement placement) const;

    /// Lays `piece` at its placement, ignoring its height. Refuses a chassis.
    std::expected<int, PlacementFailure> Put(const CarPiece& piece);

    std::expected<CarPiece, PlacementFailure> Take(std::size_t index);

    /// Lays the piece again as the newest, or leaves it where it was on failure.
    std::expected<int, PlacementFailure> Move(std::size_t index, PiecePlacement placement);

    bool Recolour(std::size_t index, std::uint8_t colour);

    /// The newest piece filling the cell at `plate` plates up.
    [[nodiscard]] std::optional<std::size_t> PieceAt(int cellX, int cellY, int plate) const;

    [[nodiscard]] std::span<const CarPiece> Pieces() const { return pieces_; }
    [[nodiscard]] const CarGrid& Grid() const { return grid_; }

private:
    void Settle();

    CarGrid grid_{kGroundSize};
    std::vector<CarPiece> pieces_;
};

}
