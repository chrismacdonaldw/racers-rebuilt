#include "assets/token_reader.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <ranges>
#include <span>
#include <string>
#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;

Result<std::vector<Token>> ReadRest(TokenReader& reader) {
    std::vector<Token> tokens;
    while (true) {
        Result<Token> token = reader.Next();
        if (!token) {
            return std::unexpected(std::move(token.error()));
        }
        if (token->kind == TokenKind::End) {
            return tokens;
        }
        tokens.push_back(*token);
    }
}

Result<std::vector<Token>> ReadAll(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    return ReadRest(reader);
}

std::vector<TokenKind> Kinds(const std::vector<Token>& tokens) {
    return tokens | std::views::transform(&Token::kind) | std::ranges::to<std::vector>();
}

Error ReadError(std::span<const std::byte> bytes) {
    Result<std::vector<Token>> tokens = ReadAll(bytes);
    REQUIRE_FALSE(tokens.has_value());
    return tokens.error();
}

TEST_CASE("empty input is the end of data") {
    TokenReader reader({});

    CHECK(reader.Next()->kind == TokenKind::End);
    CHECK(reader.Next()->kind == TokenKind::End);
}

TEST_CASE("delimiters decode to their kinds") {
    const std::vector bytes = Bytes({0x05, 0x06, 0x07, 0x08, 0x09, 0x0A});

    const Result<std::vector<Token>> tokens = ReadAll(bytes);

    REQUIRE(tokens.has_value());
    CHECK(Kinds(*tokens) == std::vector{TokenKind::OpenBrace, TokenKind::CloseBrace,
                                        TokenKind::OpenBracket, TokenKind::CloseBracket,
                                        TokenKind::Comma, TokenKind::Semicolon});
    CHECK(tokens->at(3).offset == 3);
}

TEST_CASE("strings borrow their text from the input") {
    const std::vector bytes = Bytes({0x02, 'a', '_', 'c', 'a', 'r', 0x00, 0x02, 0x00});
    TokenReader reader(bytes);

    CHECK(reader.ReadString() == "a_car");
    CHECK(reader.ReadString() == "");
    CHECK(reader.Next()->kind == TokenKind::End);
}

TEST_CASE("32-bit values decode as int and float") {
    const std::vector bytes =
        Bytes({0x04, 0xFE, 0xFF, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0xC0, 0x3F, 0x04, 0x19, 0, 0, 0});
    TokenReader reader(bytes);

    CHECK(reader.ReadInt() == -2);
    CHECK(reader.ReadFloat() == 1.5F);
    CHECK(reader.ReadInt() == 25);
}

TEST_CASE("narrow integers widen to int") {
    const std::vector bytes = Bytes({0x0B, 0xFF, 0x0C, 0x80, 0x0D, 0xFE, 0xFF, 0x0E, 0xFE, 0xFF});
    TokenReader reader(bytes);

    CHECK(reader.ReadInt() == 255);
    CHECK(reader.ReadInt() == 128);
    CHECK(reader.ReadInt() == -2);
    CHECK(reader.ReadInt() == 65534);
}

TEST_CASE("quantized floats are scaled") {
    const std::vector bytes =
        Bytes({0x0F, 0x00, 0x10, 0x10, 0xC0, 0xFF, 0x11, 0xFD, 0xFF, 0x12, 0x7F});
    TokenReader reader(bytes);

    CHECK(reader.ReadFloat() == 1.0F);
    CHECK(reader.ReadFloat() == -2.0F);
    CHECK(reader.ReadFloat() == -3.0F);
    CHECK(reader.ReadFloat().value_or(0.0F) == doctest::Approx(1.0F));
}

TEST_CASE("fields decode from one byte or an extended id") {
    const std::vector bytes = Bytes({0x27, 0x8E, 0x13, 0x00, 0x01});

    const Result<std::vector<Token>> tokens = ReadAll(bytes);

    REQUIRE(tokens.has_value());
    REQUIRE(tokens->size() == 3);
    CHECK(tokens->at(0).field == 0x27);
    CHECK(tokens->at(1).field == 0x8E);
    CHECK(tokens->at(2).field == 0x100);
    CHECK(tokens->at(2).kind == TokenKind::Field);
}

TEST_CASE("a defined sequence expands with its values from the stream") {
    const std::vector bytes =
        Bytes({0x16, 0x17, 0x05, 0x28, 0x04, 0x04, 0x04, 0x04, 0x17, 0x19, 0, 0, 0,
               0x19, 0,    0,    0,    0x19, 0,    0,    0,    0xFF, 0,    0, 0});
    TokenReader reader(bytes);

    CHECK(reader.Next()->field == 0x28);
    CHECK(reader.ReadInt() == 25);
    CHECK(reader.ReadInt() == 25);
    CHECK(reader.ReadInt() == 25);
    CHECK(reader.ReadInt() == 255);
    CHECK(reader.Next()->kind == TokenKind::End);
}

TEST_CASE("sequences may hold extended fields") {
    const std::vector bytes = Bytes({0x16, 0x18, 0x02, 0x13, 0x34, 0x12, 0x05, 0x18, 0x18});

    const Result<std::vector<Token>> tokens = ReadAll(bytes);

    REQUIRE(tokens.has_value());
    CHECK(Kinds(*tokens) == std::vector{TokenKind::Field, TokenKind::OpenBrace, TokenKind::Field,
                                        TokenKind::OpenBrace});
    CHECK(tokens->at(2).field == 0x1234);
}

TEST_CASE("a redefined sequence replaces the old one") {
    const std::vector bytes = Bytes({0x16, 0x17, 0x01, 0x05, 0x16, 0x17, 0x01, 0x06, 0x17});

    const Result<std::vector<Token>> tokens = ReadAll(bytes);

    REQUIRE(tokens.has_value());
    CHECK(Kinds(*tokens) == std::vector{TokenKind::CloseBrace});
}

TEST_CASE("a repeated token reads its values from the stream each time") {
    const std::vector bytes = Bytes({0x14, 0x03, 0x00, 0x0B, 0x01, 0x02, 0x03, 0x06});
    TokenReader reader(bytes);

    CHECK(reader.ReadInt() == 1);
    CHECK(reader.ReadInt() == 2);
    CHECK(reader.ReadInt() == 3);
    CHECK(reader.Next()->kind == TokenKind::CloseBrace);
}

TEST_CASE("a repeated sequence expands whole each time") {
    const std::vector bytes =
        Bytes({0x16, 0x19, 0x02, 0x0B, 0x12, 0x14, 0x02, 0x00, 0x19, 0x01, 0x7F, 0x02, 0x00});
    TokenReader reader(bytes);

    CHECK(reader.ReadInt() == 1);
    CHECK(reader.ReadFloat().value_or(0.0F) == doctest::Approx(1.0F));
    CHECK(reader.ReadInt() == 2);
    CHECK(reader.ReadFloat() == 0.0F);
    CHECK(reader.Next()->kind == TokenKind::End);
}

TEST_CASE("repeats may use extended fields and a count of zero") {
    const std::vector bytes =
        Bytes({0x14, 0x02, 0x00, 0x13, 0x00, 0x02, 0x14, 0x00, 0x00, 0x04, 0x05});

    const Result<std::vector<Token>> tokens = ReadAll(bytes);

    REQUIRE(tokens.has_value());
    CHECK(Kinds(*tokens) == std::vector{TokenKind::Field, TokenKind::Field, TokenKind::OpenBrace});
    CHECK(tokens->at(1).field == 0x200);
}

TEST_CASE("nested lists and blocks decode in order") {
    const std::vector bytes = Bytes({0x27, 0x07, 0x04, 0x02, 0, 0, 0, 0x08, 0x05, 0x28, 0x05, 0x06,
                                     0x28, 0x05, 0x07, 0x08, 0x06, 0x06});
    TokenReader reader(bytes);

    CHECK(reader.Next()->field == 0x27);
    CHECK(reader.ReadListHeader() == 2);
    const Result<std::vector<Token>> rest = ReadRest(reader);
    REQUIRE(rest.has_value());
    CHECK(Kinds(*rest) == std::vector{TokenKind::Field, TokenKind::OpenBrace, TokenKind::CloseBrace,
                                      TokenKind::Field, TokenKind::OpenBrace,
                                      TokenKind::OpenBracket, TokenKind::CloseBracket,
                                      TokenKind::CloseBrace, TokenKind::CloseBrace});
}

TEST_CASE("peek returns the next token without consuming it") {
    const std::vector bytes = Bytes({0x0B, 0x07, 0x06});
    TokenReader reader(bytes);

    CHECK(reader.Peek()->kind == TokenKind::Int);
    CHECK(reader.Peek()->integer == 7);
    CHECK(reader.ReadInt() == 7);
    CHECK(reader.Peek()->kind == TokenKind::CloseBrace);
}

TEST_CASE("a token of the wrong kind names what was expected") {
    const std::vector bytes = Bytes({0x05});
    TokenReader reader(bytes);

    const Result<std::int32_t> value = reader.ReadInt();

    REQUIRE_FALSE(value.has_value());
    CHECK(value.error().code == ErrorCode::InvalidData);
    CHECK(value.error().message == "expected int at offset 0, found '{'");
}

TEST_CASE("a negative list count fails") {
    const std::vector bytes = Bytes({0x07, 0x04, 0xFF, 0xFF, 0xFF, 0xFF, 0x08, 0x05});
    TokenReader reader(bytes);

    CHECK(reader.ReadListHeader().error().code == ErrorCode::InvalidData);
}

TEST_CASE("every truncated value fails") {
    const std::vector<std::vector<std::byte>> cases{
        Bytes({0x03, 0x00, 0x00, 0x80}),
        Bytes({0x04, 0x01}),
        Bytes({0x0B}),
        Bytes({0x0C}),
        Bytes({0x0D, 0x00}),
        Bytes({0x0E}),
        Bytes({0x0F, 0x00}),
        Bytes({0x10}),
        Bytes({0x11, 0x00}),
        Bytes({0x12}),
        Bytes({0x13, 0x27}),
        Bytes({0x02, 'a', 'b'}),
        Bytes({0x14, 0x01}),
        Bytes({0x14, 0x01, 0x00}),
        Bytes({0x14, 0x01, 0x00, 0x13, 0x00}),
        Bytes({0x16, 0x17}),
        Bytes({0x16, 0x17, 0x02, 0x04}),
        Bytes({0x16, 0x17, 0x01, 0x04, 0x17, 0x01, 0x02}),
    };
    for (const std::vector<std::byte>& bytes : cases) {
        CAPTURE(bytes.size());
        CHECK(ReadError(bytes).code == ErrorCode::InvalidData);
    }
}

TEST_CASE("malformed control codes fail") {
    const std::vector<std::vector<std::byte>> cases{
        Bytes({0x00}),
        Bytes({0x01}),
        Bytes({0x15}),
        Bytes({0x17}),
        Bytes({0x14, 0x01, 0x00, 0x18}),
        Bytes({0x14, 0x01, 0x00, 0x16}),
        Bytes({0x14, 0x01, 0x00, 0x15}),
        Bytes({0x16, 0x27, 0x01, 0x04}),
        Bytes({0x16, 0x17, 0x00}),
        Bytes({0x16, 0x17, 0x11}),
        Bytes({0x16, 0x17, 0x01, 0x14}),
        Bytes({0x16, 0x17, 0x01, 0x13, 0x18, 0x00}),
        Bytes({0x13, 0x26, 0x00}),
    };
    for (const std::vector<std::byte>& bytes : cases) {
        CAPTURE(bytes.size());
        CHECK(ReadError(bytes).code == ErrorCode::InvalidData);
    }
}

TEST_CASE("strings longer than 63 bytes fail") {
    std::vector bytes = Bytes({0x02});
    bytes.resize(65, std::byte{'x'});
    bytes.push_back(std::byte{0});

    CHECK(ReadError(bytes).message == "string at offset 0 is longer than 63 bytes");
    bytes.erase(bytes.begin() + 1);
    CHECK(ReadAll(bytes).has_value());
}

}
}
