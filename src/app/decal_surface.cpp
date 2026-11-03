#include "app/decal_surface.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

namespace rr::app {
namespace {

constexpr float kMinCellSize = 16.0F;
constexpr float kMaxCellsPerSide = 256.0F;

GroundArea AreaOf(const DecalTriangle& triangle) {
    const auto [minX, maxX] = std::minmax({triangle[0].x, triangle[1].x, triangle[2].x});
    const auto [minZ, maxZ] = std::minmax({triangle[0].z, triangle[1].z, triangle[2].z});
    return {.minX = minX, .minZ = minZ, .maxX = maxX, .maxZ = maxZ};
}

GroundArea Joined(const GroundArea& a, const GroundArea& b) {
    return {.minX = std::min(a.minX, b.minX),
            .minZ = std::min(a.minZ, b.minZ),
            .maxX = std::max(a.maxX, b.maxX),
            .maxZ = std::max(a.maxZ, b.maxZ)};
}

bool Overlaps(const GroundArea& a, const GroundArea& b) {
    return a.minX <= b.maxX && b.minX <= a.maxX && a.minZ <= b.maxZ && b.minZ <= a.maxZ;
}

std::size_t CellOf(float value, float min, float cellSize, std::size_t count) {
    const float cell = std::floor((value - min) / cellSize);
    return std::min(static_cast<std::size_t>(std::max(cell, 0.0F)), count - 1);
}

std::size_t CellCount(float span, float cellSize) {
    return static_cast<std::size_t>(span / cellSize) + 1;
}

}

DecalSurface::DecalSurface(std::vector<DecalTriangle> triangles)
    : triangles_(std::move(triangles)) {
    if (triangles_.empty()) {
        return;
    }
    bounds_ = AreaOf(triangles_.front());
    for (const DecalTriangle& triangle : triangles_) {
        bounds_ = Joined(bounds_, AreaOf(triangle));
    }
    const float width = bounds_.maxX - bounds_.minX;
    const float depth = bounds_.maxZ - bounds_.minZ;
    cellSize_ = std::max(kMinCellSize, std::max(width, depth) / kMaxCellsPerSide);
    columns_ = CellCount(width, cellSize_);
    rows_ = CellCount(depth, cellSize_);
    cellStarts_.assign((columns_ * rows_) + 1, 0);
    for (const DecalTriangle& triangle : triangles_) {
        ForEachCell(CellsOf(AreaOf(triangle)), [&](std::size_t cell) { ++cellStarts_[cell + 1]; });
    }
    std::partial_sum(cellStarts_.begin(), cellStarts_.end(), cellStarts_.begin());
    cellTriangles_.resize(cellStarts_.back());
    std::vector<std::uint32_t> next(cellStarts_.begin(), cellStarts_.end() - 1);
    for (std::size_t index = 0; index < triangles_.size(); ++index) {
        ForEachCell(CellsOf(AreaOf(triangles_[index])), [&](std::size_t cell) {
            cellTriangles_[next[cell]++] = static_cast<std::uint32_t>(index);
        });
    }
}

void DecalSurface::Gather(const GroundArea& area, std::vector<std::uint32_t>& out) const {
    out.clear();
    if (triangles_.empty() || !Overlaps(area, bounds_)) {
        return;
    }
    ForEachCell(CellsOf(area), [&](std::size_t cell) {
        out.insert(out.end(), cellTriangles_.begin() + cellStarts_[cell],
                   cellTriangles_.begin() + cellStarts_[cell + 1]);
    });
    std::ranges::sort(out);
    const auto duplicates = std::ranges::unique(out);
    out.erase(duplicates.begin(), duplicates.end());
}

DecalSurface::CellRange DecalSurface::CellsOf(const GroundArea& area) const {
    return {.firstColumn = CellOf(area.minX, bounds_.minX, cellSize_, columns_),
            .firstRow = CellOf(area.minZ, bounds_.minZ, cellSize_, rows_),
            .lastColumn = CellOf(area.maxX, bounds_.minX, cellSize_, columns_),
            .lastRow = CellOf(area.maxZ, bounds_.minZ, cellSize_, rows_)};
}

}
