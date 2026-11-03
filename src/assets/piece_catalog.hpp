#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "assets/piece_library.hpp"
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

struct CatalogPartSet {
    std::size_t chassis = 0;
    std::string chassisName;
    std::vector<PartChoice> choices;
};

/// `materials` names the library's MDB materials in the order piece commands index them.
struct PieceCatalog {
    PieceLibrary library;
    std::vector<std::string> materials;
    std::vector<std::string> colours;
    std::vector<CatalogPartSet> partSets;

    /// Fails when a set's chassis, piece or colour is missing; skips pieces too big to build.
    [[nodiscard]] static Result<PieceCatalog> Load(const Vfs& vfs, PieceDetail detail);
};

/// Swaps the `.leg` extension `CRSTMGR.LEB` lists for the binary `.LEB`.
[[nodiscard]] std::string BinaryPartSetFile(std::string_view listed);

}
