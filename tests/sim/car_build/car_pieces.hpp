#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "assets/piece_library.hpp"

namespace rr::sim::test {

inline constexpr std::uint8_t kSolid = 0x80;
inline constexpr std::uint8_t kRests = 0x80;

inline assets::ShapeCell Cell(int top, std::uint8_t bottom = kRests) {
    return {.top = static_cast<std::uint8_t>(kSolid | top), .bottom = bottom};
}

inline assets::PieceRecord Piece(std::uint16_t type, int width, int depth,
                                 std::vector<assets::ShapeCell> cells) {
    return {.name = std::to_string(type),
            .type = type,
            .shape = {.size = {.width = width, .height = depth}, .cells = std::move(cells)}};
}

inline assets::PieceRecord Block(std::uint16_t type, int width, int depth, int top) {
    return Piece(
        type, width, depth,
        std::vector<assets::ShapeCell>(static_cast<std::size_t>(width * depth), Cell(top)));
}

inline assets::PieceRecord Chassis() {
    return Block(17, 10, 6, 1);
}

}
