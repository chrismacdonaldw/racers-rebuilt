#include "assets/piece_catalog.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "assets/ldraw_extra_pieces.hpp"
#include "assets/material_library.hpp"
#include "assets/merged_table.hpp"
#include "assets/part_sets.hpp"
#include "assets/piece_merge.hpp"
#include "core/log.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kPieceFolder = "MENUDATA/PIECEDB";
constexpr std::size_t kBuilderVariant = 1;
constexpr std::string_view kSetList = "CRSTMGR.LEB";

auto InFile(std::string_view file) {
    return [file](Error error) {
        error.message = std::format("{}: {}", file, error.message);
        return error;
    };
}

template <typename Parsed>
Result<Parsed> ReadPieceFile(const Vfs& vfs, std::string_view file,
                             Result<Parsed> (*parse)(std::span<const std::byte>)) {
    return vfs.Read(JoinGamePath(kPieceFolder, file)).and_then([&](const FileData& data) {
        return parse(data.Bytes()).transform_error(InFile(file));
    });
}

std::size_t PieceIndex(const PieceLibrary& library, const PieceRecord& piece) {
    return static_cast<std::size_t>(&piece - library.pieces.data());
}

Result<PartChoice> ResolveChoice(const PieceCatalog& catalog, const PartSetChoice& choice) {
    const PieceRecord* piece = catalog.library.Find(choice.piece);
    const std::string colour = ToUpperAscii(choice.colour);
    const auto found = std::ranges::find(catalog.colours, colour, ToUpperAscii);
    if (piece == nullptr || found == catalog.colours.end()) {
        return Fail(ErrorCode::NotFound,
                    std::format("no piece {} in {}", choice.piece, choice.colour));
    }
    return PartChoice{.piece = PieceIndex(catalog.library, *piece),
                      .colour = static_cast<std::uint8_t>(found - catalog.colours.begin())};
}

Result<CatalogPartSet> ResolveSet(const PieceCatalog& catalog, PartSet set) {
    const PieceRecord* chassis = catalog.library.Find(set.chassisType, kBuilderVariant);
    if (chassis == nullptr) {
        return Fail(ErrorCode::NotFound, std::format("no chassis piece {}", set.chassisType));
    }
    CatalogPartSet resolved{.chassis = PieceIndex(catalog.library, *chassis),
                            .chassisName = std::move(set.chassisName),
                            .choices = {}};
    for (const PartSetChoice& choice : set.choices) {
        Result<PartChoice> part = ResolveChoice(catalog, choice);
        if (!part) {
            return std::unexpected(std::move(part.error()));
        }
        if (!catalog.library.pieces.at(part->piece).shape.FitsWithin(kBuildGridSize)) {
            log::Warn("skipping piece {} in {}, it doesn't fit on a chassis", choice.piece,
                      resolved.chassisName);
            continue;
        }
        resolved.choices.push_back(*part);
    }
    return resolved;
}

std::vector<std::string> UniteNames(std::vector<std::vector<std::string>> lists) {
    std::vector<std::string> united;
    for (std::vector<std::string>& list : lists) {
        for (std::string& name : list) {
            const std::string upper = ToUpperAscii(name);
            if (std::ranges::find(united, upper, ToUpperAscii) == united.end()) {
                united.push_back(std::move(name));
            }
        }
    }
    return united;
}

Result<void> LoadPartSets(const Vfs& vfs, PieceCatalog& catalog) {
    Result<std::vector<std::vector<std::string>>> lists =
        ParseEveryLayer(vfs, JoinGamePath(kPieceFolder, kSetList), ParsePartSetList)
            .transform_error(InFile(kSetList));
    if (!lists) {
        return std::unexpected(std::move(lists.error()));
    }
    const std::size_t gameSets = lists->front().size();
    const std::vector<std::string> files = UniteNames(std::move(*lists));
    for (std::size_t index = 0; index < files.size(); ++index) {
        const std::string file = BinaryPartSetFile(files[index]);
        Result<CatalogPartSet> set =
            ReadPieceFile(vfs, file, ParsePartSet).and_then([&](PartSet parsed) {
                return ResolveSet(catalog, std::move(parsed)).transform_error(InFile(file));
            });
        if (set) {
            catalog.partSets.push_back(std::move(*set));
        } else if (index < gameSets) {
            return std::unexpected(std::move(set.error()));
        } else {
            log::Warn("skipping a mod's part set: {}", set.error().message);
        }
    }
    return {};
}

void AddExtraPieces(PieceCatalog& catalog) {
    for (PieceRecord& extra : ExtraPieceRecords()) {
        if (catalog.library.Find(extra.type, 0) == nullptr &&
            catalog.library.Find(extra.name) == nullptr) {
            catalog.extras.push_back({.piece = catalog.library.pieces.size()});
            catalog.library.pieces.push_back(std::move(extra));
        }
    }
}

Result<std::optional<PieceLayer>> ReadPieceLayer(const Vfs& vfs, std::size_t layer,
                                                 std::string_view stem) {
    const std::string libraryFile = std::format("{}.LEB", stem);
    const std::string materialFile = std::format("{}.MDB", stem);
    Result<std::optional<FileData>> library =
        vfs.ReadLayer(layer, JoinGamePath(kPieceFolder, libraryFile));
    if (!library || !*library) {
        return library.transform([](const auto&) { return std::optional<PieceLayer>(); });
    }
    PieceLayer read;
    Result<std::optional<FileData>> materials =
        ParsePieceLibrary((*library)->Bytes())
            .transform_error(InFile(libraryFile))
            .and_then([&](PieceLibrary parsed) {
                read.library = std::move(parsed);
                return vfs.ReadLayer(layer, JoinGamePath(kPieceFolder, materialFile));
            });
    if (!materials || !*materials) {
        return materials.transform([&](const auto&) { return std::optional(std::move(read)); });
    }
    return ParseMaterialLibrary((*materials)->Bytes())
        .transform_error(InFile(materialFile))
        .transform([&](const std::vector<MaterialAsset>& parsed) {
            for (const MaterialAsset& material : parsed) {
                read.materials.push_back(material.name);
            }
            return std::optional(std::move(read));
        });
}

Result<std::vector<PieceLayer>> ReadPieceLayers(const Vfs& vfs, std::string_view stem) {
    std::vector<PieceLayer> layers;
    for (std::size_t layer = 0; layer < vfs.LayerCount(); ++layer) {
        Result<std::optional<PieceLayer>> read = ReadPieceLayer(vfs, layer, stem);
        if (!read && layers.empty()) {
            return std::unexpected(std::move(read.error()));
        }
        if (!read) {
            log::Warn("skipping a mod's {} pieces: {}", stem, read.error().message);
        } else if (*read) {
            layers.push_back(std::move(**read));
        }
    }
    if (layers.empty() || layers.front().materials.empty()) {
        return Fail(ErrorCode::NotFound,
                    std::format("{}.LEB or its MDB isn't in the game files", stem));
    }
    return layers;
}

}

std::string BinaryPartSetFile(std::string_view listed) {
    const std::string_view stem = listed.substr(0, listed.rfind('.'));
    return ToUpperAscii(stem) + ".LEB";
}

std::span<const PartChoice> PieceCatalog::Choices(std::size_t set) const {
    if (set == ExtraSet()) {
        return extras;
    }
    return set < partSets.size() ? std::span(partSets[set].choices) : std::span<const PartChoice>();
}

const GeometryAsset* PieceCatalog::ModelOf(const PieceRecord& piece) const {
    const auto found = models.find(ToUpperAscii(piece.name));
    return found == models.end() ? nullptr : &found->second;
}

void PieceCatalog::ReadExtraModels(const Vfs& vfs) {
    std::vector<std::uint16_t> types;
    types.reserve(extras.size());
    for (const PartChoice& extra : extras) {
        types.push_back(library.pieces[extra.piece].type);
    }
    ReadExtraModels(vfs, types);
}

void PieceCatalog::ReadExtraModels(const Vfs& vfs, std::span<const std::uint16_t> types) {
    for (const PartChoice& extra : extras) {
        const PieceRecord& piece = library.pieces[extra.piece];
        if (!std::ranges::contains(types, piece.type) || ModelOf(piece) != nullptr) {
            continue;
        }
        if (std::optional<GeometryAsset> model = ReadPieceModel(vfs, piece)) {
            models.emplace(ToUpperAscii(piece.name), std::move(*model));
        }
    }
}

Result<PieceCatalog> PieceCatalog::Load(const Vfs& vfs, PieceDetail detail) {
    PieceCatalog catalog;
    const std::string_view stem = detail == PieceDetail::Menu ? "LPIECEHI" : "LPIECELO";
    return ReadPieceLayers(vfs, stem)
        .transform(MergePieceLayers)
        .and_then([&](PieceLayer merged) {
            catalog.library = std::move(merged.library);
            AddExtraPieces(catalog);
            catalog.materials = std::move(merged.materials);
            catalog.models = ReadPieceModels(vfs, catalog.library);
            return ReadPieceFile(vfs, "L_COLORS.LEB", ParsePieceColours);
        })
        .and_then([&](std::vector<std::string> colours) {
            catalog.colours = std::move(colours);
            return LoadPartSets(vfs, catalog);
        })
        .transform([&] { return std::move(catalog); });
}

}
