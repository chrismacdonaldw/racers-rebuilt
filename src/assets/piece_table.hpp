#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/mod_tables.hpp"
#include "assets/piece_library.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::string_view kPieceTablePath = "MENUDATA/PIECEDB/PIECES.TOML";

/// `set` names a part set by its file in `CRSTMGR.LEB`. An empty `colours` offers every colour.
struct ModPiece {
    std::string name;
    std::string set;
    std::vector<std::string> colours;

    friend bool operator==(const ModPiece&, const ModPiece&) = default;
};

[[nodiscard]] Result<std::vector<ModPiece>> ParsePieceTable(std::string_view text,
                                                            std::string_view sourceName);

/// Every layer's table, lowest first. A later piece named like an earlier one replaces it in place.
[[nodiscard]] Result<ModTables<ModPiece>> ReadPieceTables(const Vfs& vfs);

/// One type per piece from a hash of its name, probed past the types `library` and earlier pieces
/// take, so a piece keeps its type across machines while the mods are the same.
[[nodiscard]] std::vector<std::uint16_t> ModPieceTypes(const PieceLibrary& library,
                                                       std::span<const ModPiece> pieces);

}
