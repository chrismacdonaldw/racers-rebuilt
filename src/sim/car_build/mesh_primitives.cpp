#include "sim/car_build/mesh_primitives.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numeric>
#include <span>
#include <utility>

namespace rr::sim {
namespace {

constexpr std::uint16_t kModeMask = 0x3000;
constexpr std::uint16_t kTexturedMode = 0x1000;
constexpr std::uint16_t kQuadMode = 0x2000;
constexpr std::uint16_t kNormalPerCorner = 0x4000;
constexpr std::uint16_t kSharedNormal = 0x8000;
constexpr std::uint16_t kCommandMask = 0x0FFF;
constexpr std::uint16_t kMaterialMask = 0x07FF;
constexpr std::uint16_t kTextureBit = 0x0800;
constexpr std::uint16_t kNubCommand = 2;
constexpr std::uint32_t kFirstOwnMaterial = 3;
constexpr std::uint32_t kNubMaterial = 2;
constexpr std::uint8_t kInitialFlags = kOnLattice | kSameX | kSameY | kSameZ;
constexpr float kNormalUnit = 127.0F;
constexpr float kPlanarScale = 0.25F;

bool IsWhole(float value) {
    return std::floor(value) == value;
}

std::int8_t Negated(std::int8_t value) {
    return static_cast<std::int8_t>(-value);
}

class PieceReader {
public:
    PieceReader(const assets::PieceLibrary& library, const PieceInstance& instance,
                bool usesNubTexture)
        : library_(&library),
          indices_(library.indices),
          cursor_(instance.piece->indexOffset),
          origin_{.x = static_cast<float>(instance.placement.x),
                  .y = static_cast<float>(instance.placement.y),
                  .z = static_cast<float>(instance.height)},
          rotation_(instance.placement.rotation & 3U),
          width_(static_cast<float>(instance.piece->shape.size.width)),
          depth_(static_cast<float>(instance.piece->shape.size.height)),
          usesNubTexture_(usesNubTexture) {}

    [[nodiscard]] bool HasFailed() const { return hasFailed_; }

    std::uint16_t Next() {
        if (cursor_ >= indices_.size()) {
            hasFailed_ = true;
            return 0;
        }
        return indices_[cursor_++];
    }

    void StartTriangle(std::uint16_t command) {
        hasTextureIndex_ = (command & kModeMask) == kTexturedMode;
        hasNormalIndex_ = (command & kNormalPerCorner) != 0;
        hasSharedNormal_ = (command & kSharedNormal) != 0;
    }

    [[nodiscard]] bool ReadsNormal(int corner) const {
        return !hasSharedNormal_ && (hasNormalIndex_ || corner == 0);
    }

    [[nodiscard]] bool QuadReadsNormal() const { return hasNormalIndex_; }

    MeshVertex ReadCorner(std::uint16_t command, bool readsNormal, bool& isOnLattice) {
        const std::uint16_t position = Next();
        if (readsNormal) {
            normalIndex_ = Next();
        }
        MeshVertex vertex;
        if (position >= library_->positions.size() || normalIndex_ < 0 ||
            std::cmp_greater_equal(normalIndex_, library_->normals.size())) {
            hasFailed_ = true;
            return vertex;
        }
        const Vector3 source = library_->positions[position];
        isOnLattice = IsWhole(source.x) && IsWhole(source.y) && IsWhole(source.z);
        vertex.position = Placed(source);
        vertex.normal = Turned(library_->normals[static_cast<std::size_t>(normalIndex_)]);
        ReadTexture(command, vertex);
        return vertex;
    }

private:
    [[nodiscard]] Vector3 Placed(const Vector3& source) const {
        Vector3 placed{.z = source.z + origin_.z};
        switch (rotation_) {
            case 0:
                placed.x = source.x + origin_.x;
                placed.y = source.y + origin_.y;
                break;
            case 1:
                placed.x = source.y + origin_.x;
                placed.y = (width_ - source.x) + origin_.y;
                break;
            case 2:
                placed.x = (width_ - source.x) + origin_.x;
                placed.y = (depth_ - source.y) + origin_.y;
                break;
            default:
                placed.x = (depth_ - source.y) + origin_.x;
                placed.y = source.x + origin_.y;
                break;
        }
        return placed;
    }

    [[nodiscard]] std::array<std::int8_t, 3> Turned(const Vector3& source) const {
        const auto x = static_cast<std::int8_t>(std::lround(source.x * kNormalUnit));
        const auto y = static_cast<std::int8_t>(std::lround(source.y * kNormalUnit));
        const auto z = static_cast<std::int8_t>(std::lround(source.z * kNormalUnit));
        switch (rotation_) {
            case 0:
                return {x, y, z};
            case 1:
                return {y, Negated(x), z};
            case 2:
                return {Negated(x), Negated(y), z};
            default:
                return {Negated(y), x, z};
        }
    }

    void ReadTexture(std::uint16_t command, MeshVertex& vertex) {
        if (hasTextureIndex_) {
            const std::uint16_t index = Next();
            if (index >= library_->texCoords.size()) {
                hasFailed_ = true;
                return;
            }
            vertex.u = library_->texCoords[index].u;
            vertex.v = library_->texCoords[index].v;
        } else if (usesNubTexture_ && (command == kNubCommand || (command & kTextureBit) != 0)) {
            vertex.u = (vertex.position.x - origin_.x) * kPlanarScale;
            vertex.v = (vertex.position.y - origin_.y) * kPlanarScale;
        }
    }

    const assets::PieceLibrary* library_;
    std::span<const std::uint16_t> indices_;
    std::size_t cursor_;
    Vector3 origin_;
    unsigned rotation_;
    float width_;
    float depth_;
    bool usesNubTexture_;
    bool hasFailed_ = false;
    bool hasTextureIndex_ = false;
    bool hasNormalIndex_ = false;
    bool hasSharedNormal_ = false;
    int normalIndex_ = -1;
};

std::uint32_t MaterialOf(std::uint16_t command, std::uint32_t colourMaterial, bool usesNubTexture) {
    const std::uint32_t material = command & kMaterialMask;
    if (material >= kFirstOwnMaterial) {
        return usesNubTexture && (command & kTextureBit) != 0 ? material + 1 : material;
    }
    return usesNubTexture && material == kNubMaterial ? colourMaterial + 1 : colourMaterial;
}

void ClearFlag(std::uint8_t& flags, std::uint8_t flag) {
    flags = static_cast<std::uint8_t>(flags & ~flag);
}

void ClearDifferingAxes(const MeshVertex& corner, const MeshVertex& other, std::uint8_t& flags) {
    if (corner.position.x != other.position.x) {
        ClearFlag(flags, kSameX);
    }
    if (corner.position.y != other.position.y) {
        ClearFlag(flags, kSameY);
    }
    if (corner.position.z != other.position.z) {
        ClearFlag(flags, kSameZ);
    }
}

bool IsParallelogram(const MeshVertex& v0, const MeshVertex& v1, const MeshVertex& v2,
                     const MeshVertex& v3) {
    const auto missing = [](float a, float b, float c, float d) { return (a + b) - (c + d); };
    return missing(v1.position.x, v2.position.x, v3.position.x, v0.position.x) == 0.0F &&
           missing(v1.position.y, v2.position.y, v3.position.y, v0.position.y) == 0.0F &&
           missing(v1.position.z, v2.position.z, v3.position.z, v0.position.z) == 0.0F;
}

void ExtendToQuad(MeshPrimitives& primitives, PieceReader& reader, MeshPrimitive& triangle) {
    bool isOnLattice = true;
    const MeshVertex corner =
        reader.ReadCorner(triangle.command, reader.QuadReadsNormal(), isOnLattice);
    if (IsParallelogram(primitives.Vertex(triangle.corners[0]),
                        primitives.Vertex(triangle.corners[1]),
                        primitives.Vertex(triangle.corners[2]), corner)) {
        triangle.flags |= kParallelogram;
    }
    for (std::size_t other = 0; other < 3; ++other) {
        ClearDifferingAxes(corner, primitives.Vertex(triangle.corners[other]), triangle.flags);
    }
    if (!isOnLattice) {
        ClearFlag(triangle.flags, kOnLattice);
    }
    triangle.corners[3] = primitives.AddVertex(corner);
    triangle.cornerCount = 4;
}

void FillTriangle(MeshPrimitives& primitives, PieceReader& reader, MeshPrimitive& triangle) {
    for (std::size_t corner = 0; corner < 3; ++corner) {
        bool isOnLattice = true;
        const MeshVertex vertex = reader.ReadCorner(
            triangle.command, reader.ReadsNormal(static_cast<int>(corner)), isOnLattice);
        for (std::size_t other = 0; other < corner; ++other) {
            ClearDifferingAxes(vertex, primitives.Vertex(triangle.corners[other]), triangle.flags);
        }
        if (!isOnLattice) {
            ClearFlag(triangle.flags, kOnLattice);
        }
        triangle.corners[corner] = primitives.AddVertex(vertex);
    }
}

}

MeshPrimitives::MeshPrimitives() : pool_(kMaxMeshPrimitives), order_(kMaxMeshPrimitives) {
    Clear();
}

void MeshPrimitives::Clear() {
    vertices_.clear();
    std::ranges::iota(order_, std::size_t{0});
    live_ = 0;
    hasOverflowed_ = false;
}

Result<void> MeshPrimitives::AddPiece(const assets::PieceLibrary& library,
                                      const PieceInstance& instance, bool usesNubTexture) {
    if (live_ >= kMaxMeshPrimitives) {
        hasOverflowed_ = true;
    }
    PieceReader reader(library, instance, usesNubTexture);
    MeshPrimitive* previous = nullptr;
    for (std::uint32_t left = instance.piece->commandCount; left > 0 && !hasOverflowed_; --left) {
        const std::uint16_t command = reader.Next();
        if ((command & kModeMask) == kQuadMode) {
            if (previous == nullptr) {
                return Fail(ErrorCode::InvalidData,
                            std::format("piece {} starts with a quad", instance.piece->name));
            }
            ExtendToQuad(*this, reader, *previous);
            continue;
        }
        reader.StartTriangle(command);
        MeshPrimitive& added = At(live_);
        added = {.flags = kInitialFlags,
                 .command = static_cast<std::uint16_t>(command & kCommandMask),
                 .material = MaterialOf(command, instance.colourMaterial, usesNubTexture),
                 .part = instance.part};
        FillTriangle(*this, reader, added);
        previous = &added;
        Grow();
    }
    if (reader.HasFailed()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("piece {} indexes past the library", instance.piece->name));
    }
    return {};
}

std::uint32_t MeshPrimitives::AddVertex(const MeshVertex& vertex) {
    vertices_.push_back(vertex);
    return static_cast<std::uint32_t>(vertices_.size() - 1);
}

void MeshPrimitives::Swap(std::size_t first, std::size_t second) {
    std::swap(order_[first], order_[second]);
}

MeshPrimitive* MeshPrimitives::Duplicate(const MeshPrimitive& source, std::size_t at) {
    const std::size_t slot = order_[live_];
    order_[live_] = order_[at];
    order_[at] = slot;
    pool_[slot] = source;
    return Grow() ? &pool_[slot] : nullptr;
}

void MeshPrimitives::Remove(std::size_t slot, std::size_t lastOfGroup) {
    --live_;
    const std::size_t removed = order_[slot];
    order_[slot] = order_[lastOfGroup];
    order_[lastOfGroup] = order_[live_];
    order_[live_] = removed;
}

void MeshPrimitives::Drop(std::size_t slot) {
    --live_;
    std::swap(order_[slot], order_[live_]);
}

void MeshPrimitives::SplitQuads() {
    const std::size_t count = live_;
    for (std::size_t slot = 0; slot < count && !hasOverflowed_; ++slot) {
        MeshPrimitive& quad = At(slot);
        if (quad.cornerCount != 4) {
            continue;
        }
        quad.cornerCount = 3;
        MeshPrimitive& second = At(live_);
        second = quad;
        second.corners[0] = quad.corners[2];
        second.corners[2] = quad.corners[3];
        Grow();
    }
}

void MeshPrimitives::Sort() {
    std::stable_sort(order_.begin(), order_.begin() + static_cast<std::ptrdiff_t>(live_),
                     [&](std::size_t left, std::size_t right) {
                         const MeshPrimitive& a = pool_[left];
                         const MeshPrimitive& b = pool_[right];
                         if (a.material != b.material) {
                             return a.material < b.material;
                         }
                         return vertices_[a.corners[0]].position.z <
                                vertices_[b.corners[0]].position.z;
                     });
}

bool MeshPrimitives::Grow() {
    ++live_;
    if (live_ >= kMaxMeshPrimitives) {
        hasOverflowed_ = true;
    }
    return !hasOverflowed_;
}

}
