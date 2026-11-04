#include "assets/ldraw_pieces.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <system_error>
#include <utility>

#include "assets/piece_models.hpp"

namespace rr::assets {
namespace {

constexpr LdrawPiece Piece(std::string_view piece, std::string_view file, int quarterTurns,
                           Vector3 offset) {
    constexpr std::array<std::array<Vector3, 2>, 4> kTurns{{
        {Vector3{.x = 1.0F}, Vector3{.y = 1.0F}},
        {Vector3{.y = -1.0F}, Vector3{.x = 1.0F}},
        {Vector3{.x = -1.0F}, Vector3{.y = -1.0F}},
        {Vector3{.y = 1.0F}, Vector3{.x = -1.0F}},
    }};
    const auto& turn = kTurns.at(static_cast<std::size_t>(quarterTurns));
    return {.piece = piece,
            .file = file,
            .matrix = {.position = offset, .rows = {turn[0], turn[1], Vector3{.z = 1.0F}}}};
}

constexpr std::array kPieces{
    Piece("L287700", "2877.dat", 1, {.x = 0.5F, .y = 1.0F, .z = 1.2F}),
    Piece("L300100", "3001.dat", 1, {.x = 1.0F, .y = 2.0F, .z = 1.2F}),
    Piece("L300200", "3002.dat", 1, {.x = 1.0F, .y = 1.5F, .z = 1.2F}),
    Piece("L300300", "3003.dat", 0, {.x = 1.0F, .y = 1.0F, .z = 1.2F}),
    Piece("L300400", "3004.dat", 1, {.x = 0.5F, .y = 1.0F, .z = 1.2F}),
    Piece("L301000", "3010.dat", 1, {.x = 0.5F, .y = 2.0F, .z = 1.2F}),
    Piece("L303800", "3038.dat", 0, {.x = 1.5F, .y = 1.5F, .z = 1.2F}),
    Piece("L303900", "3039.dat", 0, {.x = 1.0F, .y = 1.5F, .z = 1.2F}),
    Piece("L304000", "3040b.dat", 0, {.x = 0.5F, .y = 1.5F, .z = 1.2F}),
    Piece("L329700", "3297.dat", 0, {.x = 2.0F, .y = 2.5F, .z = 1.2F}),
    Piece("L329800", "3298.dat", 0, {.x = 1.0F, .y = 2.5F, .z = 1.2F}),
    Piece("L362200", "3622.dat", 1, {.x = 0.5F, .y = 1.5F, .z = 1.2F}),
    Piece("L395700", "3957a.dat", 0, {.x = 0.5F, .y = 0.5F, .z = 0.4F}),
    Piece("L416100", "4161.dat", 0, {.x = 1.5F, .y = 2.5F, .z = 1.2F}),
    Piece("L428600", "4286.dat", 0, {.x = 0.5F, .y = 2.5F, .z = 1.2F}),
    Piece("L656400", "6564.dat", 2, {.x = 1.0F, .y = 1.5F, .z = 1.2F}),
    Piece("L656500", "6565.dat", 2, {.x = 1.0F, .y = 1.5F, .z = 1.2F}),
};

constexpr std::array<std::pair<std::int32_t, std::string_view>, 13> kGameColours{{
    {0, "black"},
    {1, "blue"},
    {2, "green"},
    {4, "red"},
    {6, "brown"},
    {7, "ltgray"},
    {8, "dkgray"},
    {14, "yellow"},
    {15, "white"},
    {19, "biege"},
    {70, "brown"},
    {71, "ltgray"},
    {72, "dkgray"},
}};

std::string GameMaterial(std::string_view ldrawColour) {
    std::int32_t code = kLdrawMainColour;
    const std::from_chars_result read =
        std::from_chars(ldrawColour.data(), ldrawColour.data() + ldrawColour.size(), code);
    const auto found = std::ranges::find_if(
        kGameColours, [&](const auto& colour) { return colour.first == code; });
    if (read.ec != std::errc{} || found == kGameColours.end()) {
        return std::string(kPieceColourMaterial);
    }
    return std::string(found->second);
}

bool SameName(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::toupper(static_cast<unsigned char>(x)) ==
               std::toupper(static_cast<unsigned char>(y));
    });
}

}

std::span<const LdrawPiece> LdrawPieces() {
    return kPieces;
}

std::optional<LdrawPiece> LdrawPieceFor(std::string_view pieceName) {
    const auto found = std::ranges::find_if(
        kPieces, [&](const LdrawPiece& piece) { return SameName(piece.piece, pieceName); });
    if (found == kPieces.end()) {
        return std::nullopt;
    }
    return *found;
}

Result<GeometryAsset> LoadLdrawPiece(LdrawLibrary& library, const LdrawPiece& piece) {
    Result<GeometryAsset> part = library.LoadPart(piece.file);
    if (!part) {
        return part;
    }
    const LdrawMatrix turn{.rows = piece.matrix.rows};
    for (GeometryVertex& vertex : part->vertices) {
        vertex.position = Apply(piece.matrix, vertex.position);
        vertex.normal = Apply(turn, vertex.normal);
    }
    for (std::string& material : part->materials) {
        material = GameMaterial(material);
    }
    return part;
}

}
