#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "assets/piece_library.hpp"
#include "core/vector3.hpp"

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

inline std::uint16_t AddPosition(assets::PieceLibrary& library, const Vector3& position) {
    library.positions.push_back(position);
    return static_cast<std::uint16_t>(library.positions.size() - 1);
}

inline void AddFace(assets::PieceLibrary& library, std::uint16_t command,
                    const std::array<Vector3, 3>& corners, const Vector3& normal) {
    library.normals.push_back(normal);
    const auto normalIndex = static_cast<std::uint16_t>(library.normals.size() - 1);
    const Vector3 opposite{.x = corners[1].x + corners[2].x - corners[0].x,
                           .y = corners[1].y + corners[2].y - corners[0].y,
                           .z = corners[1].z + corners[2].z - corners[0].z};
    library.indices.insert(
        library.indices.end(),
        {command, AddPosition(library, corners[0]), normalIndex, AddPosition(library, corners[1]),
         AddPosition(library, corners[2]), 0x2000, AddPosition(library, opposite)});
}

/// Gives `piece` six quad faces around its footprint, up to its first cell's top.
inline assets::PieceRecord WithBox(assets::PieceLibrary& library, assets::PieceRecord piece,
                                   std::uint16_t topCommand = 0) {
    const auto w = static_cast<float>(piece.shape.size.width);
    const auto d = static_cast<float>(piece.shape.size.height);
    const auto h = static_cast<float>(piece.shape.cells.front().TopHeight());
    piece.indexOffset = static_cast<std::uint32_t>(library.indices.size());
    piece.commandCount = 12;
    AddFace(library, 0, {{{}, {.x = w}, {.y = d}}}, {.z = -1.0F});
    AddFace(library, topCommand, {{{.z = h}, {.x = w, .z = h}, {.y = d, .z = h}}}, {.z = 1.0F});
    AddFace(library, 0, {{{}, {.y = d}, {.z = h}}}, {.x = -1.0F});
    AddFace(library, 0, {{{.x = w}, {.x = w, .y = d}, {.x = w, .z = h}}}, {.x = 1.0F});
    AddFace(library, 0, {{{}, {.x = w}, {.z = h}}}, {.y = -1.0F});
    AddFace(library, 0, {{{.y = d}, {.x = w, .y = d}, {.y = d, .z = h}}}, {.y = 1.0F});
    return piece;
}

}
