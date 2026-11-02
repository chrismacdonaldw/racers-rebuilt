#include "assets/piece_library.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/token_schema.hpp"
#include "core/assert.hpp"

namespace rr::assets {
namespace {

constexpr float kPositionScale = 1.0F / 256.0F;
constexpr float kNormalScale = 1.0F / 127.0F;
constexpr float kTexCoordScale = 1.0F / 1024.0F;

struct StoredPiece {
    std::string name;
    std::uint16_t type = 0;
    std::int32_t shapeIndex = 0;
    std::int32_t commandCount = 0;
    std::int32_t indexOffset = 0;
};

struct StoredLibrary {
    PieceLibrary library;
    std::vector<StoredPiece> pieces;
    std::vector<ShapeCell> cells;
};

template <typename Int>
Result<Int> ReadInteger(TokenReader& reader) {
    return reader.ReadInt().and_then([](std::int32_t value) -> Result<Int> {
        if (!std::in_range<Int>(value)) {
            return Fail(ErrorCode::InvalidData,
                        std::format("{} is out of range in the piece library", value));
        }
        return static_cast<Int>(value);
    });
}

template <typename Int, std::size_t N>
Result<std::array<Int, N>> ReadIntegers(TokenReader& reader) {
    std::array<Int, N> values{};
    for (Int& value : values) {
        Result<Int> read = ReadInteger<Int>(reader);
        if (!read) {
            return std::unexpected(std::move(read.error()));
        }
        value = *read;
    }
    return values;
}

template <typename Int, std::size_t N, typename Element>
Result<void> ReadStream(TokenReader& reader, std::vector<Element>& stream,
                        Element (*convert)(const std::array<Int, N>&)) {
    stream.clear();
    return ReadList(reader, [&] {
        return ReadIntegers<Int, N>(reader).transform(
            [&](const std::array<Int, N>& values) { stream.push_back(convert(values)); });
    });
}

std::uint16_t ToIndex(const std::array<std::uint16_t, 1>& values) {
    return values[0];
}

template <float Scale, typename Int>
float Scaled(Int value) {
    return static_cast<float>(value) * Scale;
}

Vector3 ToPosition(const std::array<std::int16_t, 3>& values) {
    return {.x = Scaled<kPositionScale>(values[0]),
            .y = Scaled<kPositionScale>(values[1]),
            .z = Scaled<kPositionScale>(values[2])};
}

Vector3 ToNormal(const std::array<std::int8_t, 3>& values) {
    return {.x = Scaled<kNormalScale>(values[0]),
            .y = Scaled<kNormalScale>(values[1]),
            .z = Scaled<kNormalScale>(values[2])};
}

TexCoord ToTexCoord(const std::array<std::int16_t, 2>& values) {
    return {.u = Scaled<kTexCoordScale>(values[0]), .v = Scaled<kTexCoordScale>(values[1])};
}

ShapeCell ToCell(const std::array<std::uint8_t, 2>& values) {
    return {.top = values[0], .bottom = values[1]};
}

Result<void> ReadPieces(TokenReader& reader, StoredLibrary& stored) {
    stored.pieces.clear();
    return ReadList(reader, [&] {
        return ReadMembers<&StoredPiece::name, &StoredPiece::type, &StoredPiece::shapeIndex,
                           &StoredPiece::commandCount, &StoredPiece::indexOffset>(
            reader, stored.pieces.emplace_back());
    });
}

constexpr std::array kLibraryFields{
    FieldRule<StoredLibrary>{.id = 0x27, .read = ReadPieces},
    FieldRule<StoredLibrary>{.id = 0x28,
                             .read =
                                 [](TokenReader& reader, StoredLibrary& stored) {
                                     return ReadStream(reader, stored.library.indices, ToIndex);
                                 }},
    FieldRule<StoredLibrary>{.id = 0x29,
                             .read =
                                 [](TokenReader& reader, StoredLibrary& stored) {
                                     return ReadStream(reader, stored.library.positions,
                                                       ToPosition);
                                 }},
    FieldRule<StoredLibrary>{.id = 0x2A,
                             .read =
                                 [](TokenReader& reader, StoredLibrary& stored) {
                                     return ReadStream(reader, stored.library.normals, ToNormal);
                                 }},
    FieldRule<StoredLibrary>{.id = 0x2B,
                             .read =
                                 [](TokenReader& reader, StoredLibrary& stored) {
                                     return ReadStream(reader, stored.library.texCoords,
                                                       ToTexCoord);
                                 }},
    FieldRule<StoredLibrary>{
        .id = 0x2C,
        .read = [](TokenReader& reader,
                   StoredLibrary& stored) { return ReadStream(reader, stored.cells, ToCell); }},
};

std::unexpected<Error> FailPiece(const StoredPiece& piece, std::string_view problem) {
    return Fail(ErrorCode::InvalidData, std::format("piece {} {}", piece.name, problem));
}

Result<PieceRecord> ResolvePiece(const StoredPiece& stored, const StoredLibrary& library) {
    const std::span<const ShapeCell> cells = library.cells;
    if (stored.shapeIndex < 0 || std::cmp_greater_equal(stored.shapeIndex, cells.size())) {
        return FailPiece(stored, "has no shape");
    }
    if (stored.commandCount < 0 || stored.indexOffset < 0 ||
        std::cmp_greater(stored.indexOffset, library.library.indices.size())) {
        return FailPiece(stored, "has commands outside the index stream");
    }
    const auto header = static_cast<std::size_t>(stored.shapeIndex);
    PieceRecord piece{
        .name = stored.name,
        .type = stored.type,
        .shape = {.size = {.width = cells[header].top, .height = cells[header].bottom}},
        .commandCount = static_cast<std::uint32_t>(stored.commandCount),
        .indexOffset = static_cast<std::uint32_t>(stored.indexOffset),
    };
    const std::size_t count = cells[header].top * std::size_t{cells[header].bottom};
    const std::size_t extra = piece.IsChassis() ? 1 : 0;
    if (count == 0 || header + 1 + count + extra > cells.size()) {
        return FailPiece(stored, "has a shape outside the cell list");
    }
    const std::span<const ShapeCell> shape = cells.subspan(header + 1, count);
    piece.shape.cells.assign(shape.begin(), shape.end());
    if (piece.IsChassis()) {
        const ShapeCell partType = cells[header + 1 + count];
        const auto position = static_cast<std::uint16_t>(partType.top | (partType.bottom << 8U));
        if (position >= library.library.positions.size()) {
            return FailPiece(stored, "has a model offset past the positions");
        }
        piece.modelOffsetPosition = position;
    }
    return piece;
}

}

CellSize PieceShape::Footprint(unsigned rotation) const {
    return (rotation & 1U) == 0 ? size : CellSize{.width = size.height, .height = size.width};
}

ShapeCell PieceShape::GetCell(int x, int y, unsigned rotation) const {
    const CellSize footprint = Footprint(rotation);
    RR_ASSERT(x >= 0 && x < footprint.width && y >= 0 && y < footprint.height);
    const int w = size.width;
    const int h = size.height;
    int index = 0;
    switch (rotation & 3U) {
        case 0:
            index = (x * h) + y;
            break;
        case 1:
            index = x + (h * (w - y - 1));
            break;
        case 2:
            index = ((w - x - 1) * h) + (h - y - 1);
            break;
        default:
            index = (y * h) + (h - x - 1);
            break;
    }
    return cells[static_cast<std::size_t>(index)];
}

const PieceRecord* PieceLibrary::Find(std::uint16_t type, std::size_t variant) const {
    const PieceRecord* found = nullptr;
    std::size_t seen = 0;
    for (const PieceRecord& piece : pieces) {
        if (piece.type != type) {
            continue;
        }
        found = &piece;
        if (seen++ == variant) {
            break;
        }
    }
    return found;
}

const PieceRecord* PieceLibrary::Find(std::string_view name) const {
    const std::string wanted = ToUpperAscii(name);
    for (const PieceRecord& piece : pieces) {
        if (ToUpperAscii(piece.name) == wanted) {
            return &piece;
        }
    }
    return nullptr;
}

Result<PieceLibrary> ParsePieceLibrary(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    StoredLibrary stored;
    Result<void> read =
        ReadFields<StoredLibrary>(reader, kLibraryFields, stored, FieldRepeats::Fail)
            .and_then([&] { return reader.Expect(TokenKind::End); })
            .transform([](const Token&) {});
    if (!read) {
        return std::unexpected(std::move(read.error()));
    }
    if (stored.pieces.empty()) {
        return Fail(ErrorCode::InvalidData, "the piece library has no pieces");
    }
    for (const StoredPiece& piece : stored.pieces) {
        Result<PieceRecord> resolved = ResolvePiece(piece, stored);
        if (!resolved) {
            return std::unexpected(std::move(resolved.error()));
        }
        stored.library.pieces.push_back(std::move(*resolved));
    }
    std::ranges::stable_sort(stored.library.pieces, {}, [](const PieceRecord& piece) {
        return std::pair{piece.type, piece.commandCount};
    });
    return std::move(stored.library);
}

}
