#include "sim/car_build/built_car.hpp"

#include <algorithm>
#include <format>

namespace rr::sim {
namespace {

constexpr std::size_t kPieceVariant = 1;
constexpr std::size_t kEntrySize = 8;
constexpr float kCentreX = 4.5F;
constexpr float kCentreY = 2.5F;

unsigned ByteAt(std::span<const std::byte> data, std::size_t offset) {
    return std::to_integer<unsigned>(data[offset]);
}

std::uint16_t U16At(std::span<const std::byte> data, std::size_t offset) {
    return static_cast<std::uint16_t>((ByteAt(data, offset) << 8U) | ByteAt(data, offset + 1));
}

void PutU8(std::span<std::byte> data, std::size_t offset, unsigned value) {
    data[offset] = static_cast<std::byte>(value & 0xFFU);
}

void PutU16(std::span<std::byte> data, std::size_t offset, unsigned value) {
    PutU8(data, offset, value >> 8U);
    PutU8(data, offset + 1, value);
}

CarPiece EntryAt(std::span<const std::byte> entry) {
    return {.placement = {.x = static_cast<int>(ByteAt(entry, 2)),
                          .y = static_cast<int>(ByteAt(entry, 3)),
                          .rotation = ByteAt(entry, 4) & 3U},
            .colour = static_cast<std::uint8_t>(ByteAt(entry, 5)),
            .partType = U16At(entry, 6)};
}

int TopOf(const assets::PieceRecord& piece) {
    const auto tallest = std::ranges::max_element(
        piece.shape.cells, {}, [](assets::ShapeCell cell) { return cell.TopHeight(); });
    return tallest == piece.shape.cells.end() ? 0 : tallest->TopHeight();
}

struct Moments {
    int cells = 0;
    int x = 0;
    int y = 0;
    int z = 0;
};

void AddMoments(const CarPiece& entry, Moments& sum) {
    const assets::PieceShape& shape = entry.piece->shape;
    const PiecePlacement at = entry.placement;
    const assets::CellSize footprint = shape.Footprint(at.rotation);
    for (int x = 0; x < footprint.width; ++x) {
        for (int y = 0; y < footprint.height; ++y) {
            const assets::ShapeCell cell = shape.GetCell(x, y, at.rotation);
            const int top = cell.TopHeight();
            const int bottom = cell.ClampedBottomHeight();
            sum.cells += top - bottom;
            sum.x += (top - bottom) * (at.x + x);
            sum.y += (top - bottom) * (at.y + y);
            for (int z = bottom; z < top; ++z) {
                sum.z += z + entry.height;
            }
        }
    }
}

}

BuiltCar::BuiltCar() {
    pieces_.reserve(kMaxCarPieces);
}

Result<BuiltCar> BuiltCar::Load(std::span<const std::byte> data,
                                const assets::PieceLibrary& library) {
    if (data.size() < 2) {
        return Fail(ErrorCode::InvalidData, "the car data has no piece count");
    }
    const std::size_t count = U16At(data, 0);
    if (count > kMaxCarPieces) {
        return Fail(ErrorCode::InvalidData,
                    std::format("a car holds at most {} pieces, not {}", kMaxCarPieces, count));
    }
    if (data.size() < 2 + (count * kEntrySize)) {
        return Fail(ErrorCode::InvalidData,
                    std::format("the car data ends before its {} pieces", count));
    }
    BuiltCar build;
    for (std::size_t entry = 0; entry < count; ++entry) {
        const std::span<const std::byte> bytes = data.subspan(2 + (entry * kEntrySize), kEntrySize);
        CarPiece added = EntryAt(bytes);
        added.piece = library.Find(U16At(bytes, 0), kPieceVariant);
        if (added.piece == nullptr) {
            continue;
        }
        const std::expected<int, PlacementFailure> height =
            build.grid_.RestingHeight(*added.piece, added.placement, false);
        if (height && *height + TopOf(*added.piece) > kMaxCarHeight) {
            continue;
        }
        if (!build.Add(added, false)) {
            build.Add(added, true);
        }
    }
    return build;
}

assets::CarData BuiltCar::Save() const {
    assets::CarData data{};
    PutU16(data, 0, static_cast<unsigned>(pieces_.size()));
    for (std::size_t index = 0; index < pieces_.size(); ++index) {
        const CarPiece& entry = pieces_[index];
        const std::span<std::byte> bytes =
            std::span(data).subspan(2 + (index * kEntrySize), kEntrySize);
        PutU16(bytes, 0, entry.piece->type);
        PutU8(bytes, 2, static_cast<unsigned>(entry.placement.x));
        PutU8(bytes, 3, static_cast<unsigned>(entry.placement.y));
        PutU8(bytes, 4, entry.placement.rotation);
        PutU8(bytes, 5, entry.colour);
        PutU16(bytes, 6, entry.partType);
    }
    return data;
}

std::expected<int, PlacementFailure> BuiltCar::Test(const assets::PieceRecord& piece,
                                                    PiecePlacement placement) const {
    const std::expected<int, PlacementFailure> height =
        grid_.RestingHeight(piece, placement, piece.IsChassis());
    if (!height) {
        return height;
    }
    if (grid_.Collides(piece, placement, *height)) {
        return std::unexpected(PlacementFailure::Collision);
    }
    if (*height + TopOf(piece) > kMaxCarHeight) {
        return std::unexpected(PlacementFailure::TooHigh);
    }
    if (pieces_.size() >= kMaxCarPieces) {
        return std::unexpected(PlacementFailure::TooManyPieces);
    }
    return height;
}

std::expected<int, PlacementFailure> BuiltCar::Place(const assets::PieceRecord& piece,
                                                     PiecePlacement placement, std::uint8_t colour,
                                                     std::uint16_t partType) {
    PiecePlacement turned = placement;
    turned.rotation &= 3U;
    const std::expected<int, PlacementFailure> height = Test(piece, turned);
    if (height) {
        Add({.piece = &piece, .placement = turned, .colour = colour, .partType = partType},
            piece.IsChassis());
    }
    return height;
}

std::optional<CarPiece> BuiltCar::Undo() {
    if (pieces_.size() <= 1) {
        return std::nullopt;
    }
    const CarPiece newest = pieces_.back();
    pieces_.pop_back();
    grid_.Clear();
    for (std::size_t index = 0; index < pieces_.size(); ++index) {
        const CarPiece& entry = pieces_[index];
        grid_.Stamp(*entry.piece, entry.placement, entry.height, static_cast<int>(index));
    }
    return newest;
}

void BuiltCar::Clear() {
    pieces_.clear();
    grid_.Clear();
}

BrickVolume BuiltCar::Bricks() const {
    Moments sum;
    for (const CarPiece& entry : pieces_) {
        if (!entry.piece->IsChassis()) {
            AddMoments(entry, sum);
        }
    }
    if (sum.cells == 0) {
        return {};
    }
    const auto cells = static_cast<float>(sum.cells);
    return {.cells = sum.cells,
            .centroid = {.x = (static_cast<float>(sum.x) / cells) - kCentreX,
                         .y = (static_cast<float>(sum.y) / cells) - kCentreY,
                         .z = static_cast<float>(sum.z) / cells}};
}

bool BuiltCar::Add(const CarPiece& piece, bool allowsEmpty) {
    const std::expected<int, PlacementFailure> height =
        grid_.RestingHeight(*piece.piece, piece.placement, allowsEmpty);
    if (!height || grid_.Collides(*piece.piece, piece.placement, *height)) {
        return false;
    }
    const auto index = static_cast<int>(pieces_.size());
    CarPiece& added = pieces_.emplace_back(piece);
    added.height = *height;
    grid_.Stamp(*added.piece, added.placement, added.height, index);
    return true;
}

}
