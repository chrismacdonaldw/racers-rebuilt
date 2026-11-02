#include "assets/loading_screen.hpp"

#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kScreenField = 0x27;

constexpr std::array kScreenFields{
    Field<&LoadingScreenAsset::image>(0x28),
    Field<&LoadingScreenAsset::ticks>(0x29),
    Field<&LoadingScreenAsset::nameIndex>(0x2A),
};

}

Result<LoadingScreenAsset> ParseLsb(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    LoadingScreenAsset screen;
    return ExpectField(reader, kScreenField)
        .and_then([&] { return ReadBlock<LoadingScreenAsset>(reader, kScreenFields, screen); })
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .transform([&](const Token&) { return std::move(screen); });
}

}
