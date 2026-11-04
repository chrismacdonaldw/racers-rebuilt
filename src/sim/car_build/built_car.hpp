#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

#include "assets/car_data.hpp"
#include "assets/piece_library.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "sim/car_build/car_grid.hpp"

namespace rr::sim {

inline constexpr std::size_t kMaxCarPieces = 64;
inline constexpr int kMaxCarHeight = 15;

/// `height` is in plates. `partType` is the chassis type of the part set it came from, or 0.
struct CarPiece {
    const assets::PieceRecord* piece = nullptr;
    PiecePlacement placement;
    int height = 0;
    std::uint8_t colour = 0;
    std::uint16_t partType = 0;
};

/// `centroid` is in cells across and deep from the grid's centre, and plates up.
struct BrickVolume {
    int cells = 0;
    Vector3 centroid;
};

/// In plates.
[[nodiscard]] int TopOf(const assets::PieceRecord& piece);

/// Whether `entry` fills the cell at `plate` plates up.
[[nodiscard]] bool Fills(const CarPiece& entry, int cellX, int cellY, int plate);

/// A car on the builder grid, its pieces pointing into their library. The first is the chassis.
class BuiltCar {
public:
    BuiltCar();

    /// Skips pieces the library lacks or that rise past 15 plates, as the original does.
    [[nodiscard]] static Result<BuiltCar> Load(std::span<const std::byte> data,
                                               const assets::PieceLibrary& library);

    [[nodiscard]] assets::CarData Save() const;

    /// The height in plates the piece would rest at.
    [[nodiscard]] std::expected<int, PlacementFailure> Test(const assets::PieceRecord& piece,
                                                            PiecePlacement placement) const;

    std::expected<int, PlacementFailure> Place(const assets::PieceRecord& piece,
                                               PiecePlacement placement, std::uint8_t colour,
                                               std::uint16_t partType);

    /// Takes the newest piece back off for the cursor. The chassis stays.
    std::optional<CarPiece> Undo();

    /// The later pieces that would no longer rest where they are without piece `index`.
    [[nodiscard]] std::vector<std::size_t> Supports(std::size_t index) const;

    /// Refused for the chassis and for a piece that `Supports` others.
    std::expected<CarPiece, PlacementFailure> Remove(std::size_t index);

    /// Puts back a piece `Remove` took, at its old index and height.
    void Insert(std::size_t index, const CarPiece& piece);

    /// Places the piece again as the newest, or leaves it where it was on failure.
    std::expected<int, PlacementFailure> Move(std::size_t index, PiecePlacement placement);

    bool Recolour(std::size_t index, std::uint8_t colour);

    /// The newest piece filling the cell at `plate` plates up.
    [[nodiscard]] std::optional<std::size_t> PieceAt(int cellX, int cellY, int plate) const;

    void Clear();

    [[nodiscard]] std::span<const CarPiece> Pieces() const { return pieces_; }

    [[nodiscard]] const CarGrid& Grid() const { return grid_; }

    /// Sums every brick's cells, leaving out the chassis.
    [[nodiscard]] BrickVolume Bricks() const;

private:
    bool Add(const CarPiece& piece, bool allowsEmpty);
    void Restamp();

    CarGrid grid_;
    std::vector<CarPiece> pieces_;
};

}
