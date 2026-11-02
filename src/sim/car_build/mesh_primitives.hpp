#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "assets/piece_library.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "sim/car_build/car_grid.hpp"

namespace rr::sim {

inline constexpr std::size_t kMaxMeshPrimitives = 2500;

inline constexpr std::uint8_t kOnLattice = 0x01;
inline constexpr std::uint8_t kSameX = 0x02;
inline constexpr std::uint8_t kSameY = 0x04;
inline constexpr std::uint8_t kSameZ = 0x08;
inline constexpr std::uint8_t kParallelogram = 0x80;

/// `position` is in cells across and plates up until the build scales it. `normal` is in 127ths.
struct MeshVertex {
    Vector3 position;
    std::array<std::int8_t, 3> normal{};
    float u = 0.0F;
    float v = 0.0F;
};

/// A quad's fourth corner sits across from its first. `part` is the piece's index in the car.
struct MeshPrimitive {
    std::array<std::uint32_t, 4> corners{};
    std::uint8_t cornerCount = 3;
    std::uint8_t flags = 0;
    std::uint16_t command = 0;
    std::uint32_t material = 0;
    std::size_t part = 0;
};

struct PieceInstance {
    const assets::PieceRecord* piece = nullptr;
    PiecePlacement placement;
    int height = 0;
    std::uint32_t colourMaterial = 0;
    std::size_t part = 0;
};

/// Primitives by slot, so a split or removal only moves slots. The first `Live()` are drawn.
class MeshPrimitives {
public:
    MeshPrimitives();

    void Clear();

    /// `usesNubTexture` swaps in the textured nub. Fails on indices past the library's streams.
    Result<void> AddPiece(const assets::PieceLibrary& library, const PieceInstance& instance,
                          bool usesNubTexture);

    [[nodiscard]] bool HasOverflowed() const { return hasOverflowed_; }
    [[nodiscard]] std::size_t Live() const { return live_; }

    [[nodiscard]] MeshPrimitive& At(std::size_t slot) { return pool_[order_[slot]]; }
    [[nodiscard]] const MeshPrimitive& At(std::size_t slot) const { return pool_[order_[slot]]; }
    [[nodiscard]] const MeshVertex& Vertex(std::uint32_t index) const { return vertices_[index]; }
    [[nodiscard]] std::size_t VertexCount() const { return vertices_.size(); }

    std::uint32_t AddVertex(const MeshVertex& vertex);
    void Swap(std::size_t first, std::size_t second);

    /// Copies `source` to slot `at`, moving that slot's primitive to the end. Null on overflow.
    MeshPrimitive* Duplicate(const MeshPrimitive& source, std::size_t at);

    /// Moves the primitive at `slot` past the live ones, filling its slot from `lastOfGroup`.
    void Remove(std::size_t slot, std::size_t lastOfGroup);

    /// Moves the primitive at `slot` past the live ones, filling its slot from the last live one.
    void Drop(std::size_t slot);

    void SplitQuads();

    /// By material, then by first corner height.
    void Sort();

private:
    std::vector<MeshVertex> vertices_;
    std::vector<MeshPrimitive> pool_;
    std::vector<std::size_t> order_;
    std::size_t live_ = 0;
    bool hasOverflowed_ = false;

    bool Grow();
};

}
