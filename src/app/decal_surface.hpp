#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/vector3.hpp"

namespace rr::app {

using DecalTriangle = std::array<Vector3, 3>;

/// In engine x and z.
struct GroundArea {
    float minX = 0.0F;
    float minZ = 0.0F;
    float maxX = 0.0F;
    float maxZ = 0.0F;
};

/// Track triangles in engine space that take shadows and skid marks, gridded by ground area.
class DecalSurface {
public:
    DecalSurface() = default;
    explicit DecalSurface(std::vector<DecalTriangle> triangles);

    [[nodiscard]] std::span<const DecalTriangle> Triangles() const { return triangles_; }

    /// Overwrites `out` with the index of each triangle whose ground area may meet `area`, once.
    void Gather(const GroundArea& area, std::vector<std::uint32_t>& out) const;

private:
    struct CellRange {
        std::size_t firstColumn = 0;
        std::size_t firstRow = 0;
        std::size_t lastColumn = 0;
        std::size_t lastRow = 0;
    };

    [[nodiscard]] CellRange CellsOf(const GroundArea& area) const;

    template <typename Visit>
    void ForEachCell(const CellRange& range, Visit visit) const {
        for (std::size_t row = range.firstRow; row <= range.lastRow; ++row) {
            for (std::size_t column = range.firstColumn; column <= range.lastColumn; ++column) {
                visit((row * columns_) + column);
            }
        }
    }

    std::vector<DecalTriangle> triangles_;
    GroundArea bounds_;
    float cellSize_ = 1.0F;
    std::size_t columns_ = 0;
    std::size_t rows_ = 0;
    std::vector<std::uint32_t> cellStarts_;
    std::vector<std::uint32_t> cellTriangles_;
};

}
