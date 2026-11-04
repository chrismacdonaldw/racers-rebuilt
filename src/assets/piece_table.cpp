#include "assets/piece_table.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

#include "assets/game_path.hpp"
#include "assets/ldraw_extra_pieces.hpp"
#include "assets/toml_entries.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kPiecesKey = "piece";
constexpr std::size_t kMostColours = 64;
constexpr std::uint32_t kModTypeCount = 0x10000U - kFirstModPieceType;
constexpr std::uint32_t kHashOffset = 2166136261U;
constexpr std::uint32_t kHashPrime = 16777619U;

bool IsPieceName(std::string_view name) {
    return !name.empty() && std::ranges::all_of(name, [](char letter) {
        return (letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z') ||
               (letter >= '0' && letter <= '9') || letter == '-' || letter == '_';
    });
}

std::uint32_t NameHash(std::string_view name) {
    std::uint32_t hash = kHashOffset;
    for (const char letter : ToUpperAscii(name)) {
        hash = (hash ^ static_cast<unsigned char>(letter)) * kHashPrime;
    }
    return hash;
}

Result<ModPiece> ParsePiece(const toml::table& table) {
    TomlFields fields(table);
    ModPiece piece{.name = fields.Text("name").value_or(std::string()),
                   .set = fields.Text("set").value_or(std::string()),
                   .colours = fields.TextList("colours", kMostColours, "colour names")
                                  .value_or(std::vector<std::string>())};
    if (!IsPieceName(piece.name)) {
        fields.Complain("name must be letters, digits, - or _");
    } else if (piece.set.empty()) {
        fields.Complain("set must name a part set");
    }
    if (fields.Problem()) {
        return Fail(ErrorCode::InvalidData, *fields.Problem());
    }
    return piece;
}

std::vector<ModPiece> MergeByName(std::vector<ModPiece> pieces) {
    std::vector<ModPiece> merged;
    for (ModPiece& piece : pieces) {
        const auto same = std::ranges::find_if(merged, [&](const ModPiece& earlier) {
            return ToUpperAscii(earlier.name) == ToUpperAscii(piece.name);
        });
        if (same == merged.end()) {
            merged.push_back(std::move(piece));
        } else {
            *same = std::move(piece);
        }
    }
    return merged;
}

}

Result<std::vector<ModPiece>> ParsePieceTable(std::string_view text, std::string_view sourceName) {
    return ParseTomlEntries<ModPiece>(text, sourceName, kPiecesKey, ParsePiece);
}

Result<ModTables<ModPiece>> ReadPieceTables(const Vfs& vfs) {
    return ReadModTables(vfs, kPieceTablePath, ParsePieceTable).transform([](auto tables) {
        tables.entries = MergeByName(std::move(tables.entries));
        return tables;
    });
}

std::vector<std::uint16_t> ModPieceTypes(const PieceLibrary& library,
                                         std::span<const ModPiece> pieces) {
    std::vector<std::uint16_t> types;
    types.reserve(pieces.size());
    const auto isTaken = [&](std::uint16_t type) {
        return library.Find(type, 0) != nullptr || std::ranges::contains(types, type);
    };
    for (const ModPiece& piece : pieces) {
        std::uint32_t slot = NameHash(piece.name) % kModTypeCount;
        auto type = static_cast<std::uint16_t>(kFirstModPieceType + slot);
        for (std::uint32_t tried = 1; isTaken(type) && tried < kModTypeCount; ++tried) {
            slot = (slot + 1) % kModTypeCount;
            type = static_cast<std::uint16_t>(kFirstModPieceType + slot);
        }
        types.push_back(type);
    }
    return types;
}

}
