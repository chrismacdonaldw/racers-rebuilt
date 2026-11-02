#include "assets/piece_catalog.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <span>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/part_sets.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kPieceFolder = "MENUDATA/PIECEDB";
constexpr std::size_t kBuilderVariant = 1;

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
        resolved.choices.push_back(*part);
    }
    return resolved;
}

Result<void> LoadPartSets(const Vfs& vfs, PieceCatalog& catalog) {
    Result<std::vector<std::string>> files = ReadPieceFile(vfs, "CRSTMGR.LEB", ParsePartSetList);
    if (!files) {
        return std::unexpected(std::move(files.error()));
    }
    for (const std::string& listed : *files) {
        const std::string file = BinaryPartSetFile(listed);
        Result<CatalogPartSet> set =
            ReadPieceFile(vfs, file, ParsePartSet).and_then([&](PartSet parsed) {
                return ResolveSet(catalog, std::move(parsed)).transform_error(InFile(file));
            });
        if (!set) {
            return std::unexpected(std::move(set.error()));
        }
        catalog.partSets.push_back(std::move(*set));
    }
    return {};
}

}

std::string BinaryPartSetFile(std::string_view listed) {
    const std::string_view stem = listed.substr(0, listed.rfind('.'));
    return ToUpperAscii(stem) + ".LEB";
}

Result<PieceCatalog> PieceCatalog::Load(const Vfs& vfs, PieceDetail detail) {
    PieceCatalog catalog;
    const std::string_view libraryFile =
        detail == PieceDetail::Menu ? "LPIECEHI.LEB" : "LPIECELO.LEB";
    return ReadPieceFile(vfs, libraryFile, ParsePieceLibrary)
        .and_then([&](PieceLibrary library) {
            catalog.library = std::move(library);
            return ReadPieceFile(vfs, "L_COLORS.LEB", ParsePieceColours);
        })
        .and_then([&](std::vector<std::string> colours) {
            catalog.colours = std::move(colours);
            return LoadPartSets(vfs, catalog);
        })
        .transform([&] { return std::move(catalog); });
}

}
