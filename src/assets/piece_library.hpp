#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

inline constexpr std::uint16_t kStudPieceType = 0x800;

/// Heights are in plates.
struct ShapeCell {
    std::uint8_t top = 0;
    std::uint8_t bottom = 0;

    [[nodiscard]] bool Occupied() const { return (top & 0x80U) != 0; }
    [[nodiscard]] int TopHeight() const { return top & 0x3F; }
    [[nodiscard]] int BottomHeight() const { return bottom & 0x3F; }
    [[nodiscard]] bool RestsOnBelow() const { return (bottom & 0x80U) != 0; }
    [[nodiscard]] bool CanSitLevel() const { return (bottom & 0xC0U) != 0; }
    [[nodiscard]] bool IsEmpty() const { return TopHeight() == 0 && BottomHeight() == 0; }

    /// Zero when the bottom is above the top.
    [[nodiscard]] int ClampedBottomHeight() const {
        return BottomHeight() > TopHeight() ? 0 : BottomHeight();
    }

    friend bool operator==(const ShapeCell&, const ShapeCell&) = default;
};

struct CellSize {
    int width = 0;
    int height = 0;

    friend bool operator==(const CellSize&, const CellSize&) = default;
};

/// Rotations count quarter turns. Only their low two bits count.
struct PieceShape {
    CellSize size;
    std::vector<ShapeCell> cells;

    [[nodiscard]] CellSize Footprint(unsigned rotation) const;

    /// `x` and `y` are inside `Footprint(rotation)`.
    [[nodiscard]] ShapeCell GetCell(int x, int y, unsigned rotation) const;
};

struct PieceRecord {
    std::string name;
    std::uint16_t type = 0;
    PieceShape shape;
    std::optional<std::uint16_t> modelOffsetPosition;
    std::uint32_t commandCount = 0;
    std::uint32_t indexOffset = 0;

    [[nodiscard]] bool IsChassis() const { return type < kStudPieceType; }
};

struct TexCoord {
    float u = 0.0F;
    float v = 0.0F;

    friend bool operator==(const TexCoord&, const TexCoord&) = default;
};

/// Pieces are sorted by type, then by command count, which orders a type's variants. Positions are
/// in cells across and plates up.
struct PieceLibrary {
    std::vector<PieceRecord> pieces;
    std::vector<std::uint16_t> indices;
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<TexCoord> texCoords;

    /// Clamps `variant` to the variants of `type`.
    [[nodiscard]] const PieceRecord* Find(std::uint16_t type, std::size_t variant) const;

    /// Matches `name` in any case.
    [[nodiscard]] const PieceRecord* Find(std::string_view name) const;
};

[[nodiscard]] Result<PieceLibrary> ParsePieceLibrary(std::span<const std::byte> bytes);

}
