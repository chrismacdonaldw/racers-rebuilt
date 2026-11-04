#include "assets/piece_catalog.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "assets/derived_shape.hpp"
#include "assets/game_path.hpp"
#include "assets/ldraw_extra_pieces.hpp"
#include "assets/material_library.hpp"
#include "assets/merged_table.hpp"
#include "assets/part_sets.hpp"
#include "assets/piece_merge.hpp"
#include "assets/piece_table.hpp"
#include "core/log.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kPieceFolder = "MENUDATA/PIECEDB";
constexpr std::size_t kBuilderVariant = 1;
constexpr std::string_view kSetList = "CRSTMGR.LEB";
constexpr std::string_view kMenuStem = "LPIECEHI";
constexpr std::string_view kRaceStem = "LPIECELO";
constexpr std::size_t kColourLimit = 256;

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
                            .choices = {},
                            .name = {}};
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
            set->name = file.substr(0, file.rfind('.'));
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

std::string ModProblem(const ModPiece& piece, std::string_view problem) {
    return std::format("{}: piece {}: {}", kPieceTablePath, piece.name, problem);
}

class ModPieceCache {
public:
    static ModPieceCache& Get() {
        static ModPieceCache cache;
        return cache;
    }

    ModTables<ModPiece> Tables(const Vfs& vfs) {
        const std::scoped_lock lock(mutex_);
        Refresh(vfs);
        return tables_;
    }

    std::optional<GeometryAsset> Model(const Vfs& vfs, const PieceRecord& piece) {
        const std::scoped_lock lock(mutex_);
        Refresh(vfs);
        const std::string key = ToUpperAscii(piece.name);
        auto found = models_.find(key);
        if (found == models_.end()) {
            found = models_.emplace(key, ReadPieceModel(vfs, piece)).first;
        }
        return found->second;
    }

private:
    void Refresh(const Vfs& vfs) {
        if (vfs.Generation() == generation_) {
            return;
        }
        generation_ = vfs.Generation();
        models_.clear();
        Result<ModTables<ModPiece>> tables = ReadPieceTables(vfs);
        tables_ = tables ? std::move(*tables)
                         : ModTables<ModPiece>{.entries = {}, .problems = {tables.error().message}};
    }

    std::mutex mutex_;
    std::uint64_t generation_ = 0;
    ModTables<ModPiece> tables_;
    std::map<std::string, std::optional<GeometryAsset>, std::less<>> models_;
};

std::vector<ModPiece> ReadModPieces(const Vfs& vfs, std::vector<std::string>& problems) {
    ModTables<ModPiece> tables = ModPieceCache::Get().Tables(vfs);
    std::ranges::move(tables.problems, std::back_inserter(problems));
    return std::move(tables.entries);
}

bool HasModel(const Vfs& vfs, const ModPiece& piece) {
    const std::string path = PieceModelPath(piece.name);
    return vfs.Contains(path) || vfs.Contains(path.substr(0, path.rfind('.')) + ".GLB");
}

Result<PieceRecord> ModPieceRecord(const Vfs& vfs, PieceCatalog& catalog, const ModPiece& piece,
                                   std::uint16_t type) {
    if (catalog.library.Find(piece.name) != nullptr) {
        return Fail(ErrorCode::InvalidData, "another piece has its name");
    }
    PieceRecord record{.name = piece.name, .type = type};
    std::optional<GeometryAsset> model = ModPieceCache::Get().Model(vfs, record);
    if (!model) {
        return Fail(ErrorCode::NotFound,
                    std::format("there's no model at {}", PieceModelPath(piece.name)));
    }
    if (!IsOnBuildGrid(*model)) {
        return Fail(
            ErrorCode::InvalidData,
            std::format("its model must fit in {} by {} cells and {} plates from the origin",
                        kBuildGridSize.width, kBuildGridSize.height, kHighestPlate));
    }
    record.shape = DeriveShape(*model);
    catalog.models.insert_or_assign(ToUpperAscii(piece.name), std::move(*model));
    return record;
}

void AddModPieces(const Vfs& vfs, PieceCatalog& catalog, std::span<const ModPiece> pieces,
                  std::vector<std::string>& problems) {
    const std::vector<std::uint16_t> types = ModPieceTypes(catalog.library, pieces);
    for (std::size_t index = 0; index < pieces.size(); ++index) {
        Result<PieceRecord> record = ModPieceRecord(vfs, catalog, pieces[index], types[index]);
        if (record) {
            catalog.library.pieces.push_back(std::move(*record));
        } else {
            problems.push_back(ModProblem(pieces[index], record.error().message));
        }
    }
}

std::vector<std::uint8_t> ColoursOf(const PieceCatalog& catalog, const ModPiece& piece,
                                    std::vector<std::string>& problems) {
    const std::size_t offered = std::min(catalog.colours.size(), kColourLimit);
    std::vector<std::uint8_t> colours;
    for (std::size_t colour = 0; piece.colours.empty() && colour < offered; ++colour) {
        colours.push_back(static_cast<std::uint8_t>(colour));
    }
    const auto first = catalog.colours.begin();
    for (const std::string& name : piece.colours) {
        const auto found = std::ranges::find(first, first + static_cast<std::ptrdiff_t>(offered),
                                             ToUpperAscii(name), ToUpperAscii);
        if (found == first + static_cast<std::ptrdiff_t>(offered)) {
            problems.push_back(ModProblem(piece, std::format("there's no colour {}", name)));
        } else {
            colours.push_back(static_cast<std::uint8_t>(found - catalog.colours.begin()));
        }
    }
    return colours;
}

void AddModChoices(PieceCatalog& catalog, std::span<const ModPiece> pieces,
                   std::vector<std::string>& problems) {
    for (const ModPiece& piece : pieces) {
        const PieceRecord* record = catalog.library.Find(piece.name);
        if (record == nullptr || record->type < kFirstModPieceType) {
            continue;
        }
        const auto set =
            std::ranges::find(catalog.partSets, ToUpperAscii(piece.set), &CatalogPartSet::name);
        if (set == catalog.partSets.end()) {
            problems.push_back(ModProblem(piece, std::format("there's no part set {}", piece.set)));
            continue;
        }
        for (const std::uint8_t colour : ColoursOf(catalog, piece, problems)) {
            set->choices.push_back(
                {.piece = PieceIndex(catalog.library, *record), .colour = colour});
        }
    }
}

PieceLayer MergeLayers(std::vector<PieceLayer> layers, std::string_view stem,
                       std::vector<std::string>& problems) {
    std::vector<std::string> merging;
    PieceLayer merged = MergePieceLayers(std::move(layers), merging);
    for (const std::string& problem : merging) {
        problems.push_back(std::format("{}.LEB: {}", stem, problem));
    }
    return merged;
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
    std::vector<std::string> problems;
    Result<PieceCatalog> catalog = Load(vfs, detail, problems);
    for (const std::string& problem : problems) {
        log::Warn("{}, leaving it out", problem);
    }
    return catalog;
}

Result<PieceCatalog> PieceCatalog::Load(const Vfs& vfs, PieceDetail detail,
                                        std::vector<std::string>& problems) {
    PieceCatalog catalog;
    const std::vector<ModPiece> modPieces = ReadModPieces(vfs, problems);
    const std::string_view stem = detail == PieceDetail::Menu ? kMenuStem : kRaceStem;
    return ReadPieceLayers(vfs, stem)
        .and_then([&](std::vector<PieceLayer> layers) {
            PieceLayer merged = MergeLayers(std::move(layers), stem, problems);
            catalog.library = std::move(merged.library);
            AddExtraPieces(catalog);
            catalog.materials = std::move(merged.materials);
            catalog.models = ReadPieceModels(vfs, catalog.library);
            AddModPieces(vfs, catalog, modPieces, problems);
            return ReadPieceFile(vfs, "L_COLORS.LEB", ParsePieceColours);
        })
        .and_then([&](std::vector<std::string> colours) {
            catalog.colours = std::move(colours);
            return LoadPartSets(vfs, catalog);
        })
        .transform([&] {
            AddModChoices(catalog, modPieces, problems);
            return std::move(catalog);
        });
}

Result<std::vector<std::uint16_t>> LoadPieceTypes(const Vfs& vfs) {
    std::vector<std::string> ignored;
    const std::vector<ModPiece> modPieces = ReadModPieces(vfs, ignored);
    return ReadPieceLayers(vfs, kMenuStem).transform([&](std::vector<PieceLayer> layers) {
        const PieceLibrary library = MergePieceLayers(std::move(layers), ignored).library;
        const std::vector<std::uint16_t> modTypes = ModPieceTypes(library, modPieces);
        std::vector<std::uint16_t> types;
        for (std::size_t index = 0; index < modPieces.size(); ++index) {
            if (HasModel(vfs, modPieces[index])) {
                types.push_back(modTypes[index]);
            }
        }
        for (const PieceRecord& piece : library.pieces) {
            types.push_back(piece.type);
        }
        for (const PieceRecord& extra : ExtraPieceRecords()) {
            types.push_back(extra.type);
        }
        return types;
    });
}

}
