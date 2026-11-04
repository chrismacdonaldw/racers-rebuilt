#include "assets/ldraw_pieces.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

#include "assets/ldraw_extra_pieces.hpp"
#include "assets/piece_models.hpp"

namespace rr::assets {
namespace {

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

std::string PartMaterial(std::string_view ldrawColour, const LdrawPart& part) {
    const bool isMain = ldrawColour == std::to_string(kLdrawMainColour);
    return GameMaterial(isMain && !part.colour.empty() ? part.colour : ldrawColour);
}

bool SameName(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::toupper(static_cast<unsigned char>(x)) ==
               std::toupper(static_cast<unsigned char>(y));
    });
}

void Append(GeometryAsset& model, const GeometryAsset& part, const LdrawPart& placement) {
    const auto base = static_cast<std::uint32_t>(model.vertices.size());
    const auto firstIndex = static_cast<std::uint32_t>(model.indices.size());
    const LdrawMatrix turn{.rows = placement.matrix.rows};
    for (GeometryVertex vertex : part.vertices) {
        vertex.position = Apply(placement.matrix, vertex.position);
        vertex.normal = Apply(turn, vertex.normal);
        model.vertices.push_back(vertex);
    }
    std::ranges::transform(part.indices, std::back_inserter(model.indices),
                           [&](std::uint32_t index) { return base + index; });
    for (const GeometryRange& range : part.ranges) {
        model.ranges.push_back(
            {.firstIndex = firstIndex + range.firstIndex,
             .indexCount = range.indexCount,
             .material = FindOrAddMaterial(
                 model.materials, PartMaterial(part.materials.at(range.material), placement))});
    }
}

}

std::span<const LdrawPart> LdrawPartsOf(std::string_view pieceName) {
    const auto isPiece = [&](const LdrawPart& part) { return SameName(part.piece, pieceName); };
    for (const std::span<const LdrawPart> table : {LdrawParts(), ExtraPieceParts()}) {
        const auto first = std::ranges::find_if(table, isPiece);
        if (first != table.end()) {
            return {first, std::find_if_not(first, table.end(), isPiece)};
        }
    }
    return {};
}

Result<GeometryAsset> LoadLdrawPiece(LdrawLibrary& library, std::span<const LdrawPart> parts) {
    GeometryAsset model;
    for (const LdrawPart& part : parts) {
        const Result<GeometryAsset> loaded = library.LoadPart(part.file);
        if (!loaded) {
            return loaded;
        }
        Append(model, *loaded, part);
    }
    return model;
}

}
