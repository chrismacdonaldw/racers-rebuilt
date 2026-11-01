#include "assets/race_voices.hpp"

#include <utility>

#include "assets/token_reader.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kOpponentVoicesField = 0x3D;

}

Result<std::optional<std::string>> ReadOpponentVoices(std::span<const std::byte> rab) {
    TokenReader reader(rab);
    while (true) {
        Result<Token> token = reader.Next();
        if (!token) {
            return std::unexpected(std::move(token.error()));
        }
        if (token->kind == TokenKind::End) {
            return std::nullopt;
        }
        if (token->kind == TokenKind::Field && token->field == kOpponentVoicesField) {
            return reader.ReadString().transform(
                [](std::string_view name) { return std::optional<std::string>(name); });
        }
    }
}

}
