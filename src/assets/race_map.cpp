#include "assets/race_map.hpp"

#include <utility>

#include "assets/token_reader.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kMapWorldField = 0x45;
constexpr FieldId kMapBoundsField = 0x46;

Result<MapBounds> ReadBounds(TokenReader& reader) {
    MapBounds bounds;
    for (float* value : {&bounds.minX, &bounds.maxY, &bounds.maxX, &bounds.minY}) {
        Result<float> read = reader.ReadFloat();
        if (!read) {
            return std::unexpected(std::move(read.error()));
        }
        *value = *read;
    }
    return bounds;
}

}

Result<RaceMapFields> ReadRaceMapFields(std::span<const std::byte> rab) {
    TokenReader reader(rab);
    RaceMapFields fields;
    while (true) {
        Result<Token> token = reader.Next();
        if (!token) {
            return std::unexpected(std::move(token.error()));
        }
        if (token->kind == TokenKind::End) {
            return fields;
        }
        if (token->kind != TokenKind::Field) {
            continue;
        }
        if (token->field == kMapWorldField) {
            Result<std::string_view> name = reader.ReadString();
            if (!name) {
                return std::unexpected(std::move(name.error()));
            }
            fields.world = std::string(*name);
        } else if (token->field == kMapBoundsField) {
            Result<MapBounds> bounds = ReadBounds(reader);
            if (!bounds) {
                return std::unexpected(std::move(bounds.error()));
            }
            fields.bounds = *bounds;
        }
    }
}

}
