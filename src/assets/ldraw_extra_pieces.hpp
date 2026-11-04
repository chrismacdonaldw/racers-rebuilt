#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "assets/ldraw_pieces.hpp"
#include "assets/piece_library.hpp"

namespace rr::assets {

inline constexpr std::uint16_t kFirstExtraPieceType = 0x3000;

/// One unturned LDraw part per extra piece, in `ExtraPieceRecords` order.
[[nodiscard]] std::span<const LdrawPart> ExtraPieceParts();

/// They have no commands: their models come from `ExtraPieceParts`.
[[nodiscard]] std::vector<PieceRecord> ExtraPieceRecords();

[[nodiscard]] bool IsExtraPiece(std::string_view pieceName);

[[nodiscard]] constexpr bool IsExtraPieceType(std::uint16_t type) {
    return type >= kFirstExtraPieceType;
}

}
