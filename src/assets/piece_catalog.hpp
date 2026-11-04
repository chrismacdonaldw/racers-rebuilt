#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/geometry_asset.hpp"
#include "assets/piece_library.hpp"
#include "assets/piece_models.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::assets {

enum class PieceDetail : std::uint8_t {
    Menu,
    Race,
};

/// Indexes into the catalogue's pieces and colours.
struct PartChoice {
    std::size_t piece = 0;
    std::uint8_t colour = 0;

    friend bool operator==(const PartChoice&, const PartChoice&) = default;
};

/// `name` is the set's file in `CRSTMGR.LEB`, upper case and without its extension.
struct CatalogPartSet {
    std::size_t chassis = 0;
    std::string chassisName;
    std::vector<PartChoice> choices;
    std::string name;
};

/// `materials` names the library's MDB materials in the order piece commands index them.
// NOLINTNEXTLINE(bugprone-exception-escape)
struct PieceCatalog {
    PieceLibrary library;
    std::vector<std::string> materials;
    std::vector<std::string> colours;
    std::vector<CatalogPartSet> partSets;
    std::vector<PartChoice> extras;
    PieceModels models;

    [[nodiscard]] const GeometryAsset* ModelOf(const PieceRecord& piece) const;

    /// `Load` leaves extra pieces' models out, as reading one makes it from LDraw.
    void ReadExtraModels(const Vfs& vfs);
    void ReadExtraModels(const Vfs& vfs, std::span<const std::uint16_t> types);

    /// The set past the last part set offers the extra pieces.
    [[nodiscard]] std::size_t ExtraSet() const { return partSets.size(); }
    [[nodiscard]] std::span<const PartChoice> Choices(std::size_t set) const;

    /// Fails when a set's chassis, piece or colour is missing; skips pieces too big to build. Logs
    /// the mod pieces it leaves out.
    [[nodiscard]] static Result<PieceCatalog> Load(const Vfs& vfs, PieceDetail detail);
    /// Collects the mod pieces it leaves out instead of logging them.
    [[nodiscard]] static Result<PieceCatalog> Load(const Vfs& vfs, PieceDetail detail,
                                                   std::vector<std::string>& problems);
};

/// Every piece type a car can hold with the mounted mods, without reading any models.
[[nodiscard]] Result<std::vector<std::uint16_t>> LoadPieceTypes(const Vfs& vfs);

/// Swaps the `.leg` extension `CRSTMGR.LEB` lists for the binary `.LEB`.
[[nodiscard]] std::string BinaryPartSetFile(std::string_view listed);

}
