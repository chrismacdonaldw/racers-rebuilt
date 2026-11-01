#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "assets/byte_reader.hpp"
#include "core/result.hpp"

namespace rr::assets {

using FieldId = std::uint16_t;

enum class TokenKind : std::uint8_t {
    End,
    String,
    Float,
    Int,
    OpenBrace,
    CloseBrace,
    OpenBracket,
    CloseBracket,
    Comma,
    Semicolon,
    Field,
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::size_t offset = 0;
    std::string_view text;
    float real = 0.0F;
    std::int32_t integer = 0;
    FieldId field = 0;
};

[[nodiscard]] std::string_view ToString(TokenKind kind);

/// Strings borrow from the input bytes.
class TokenReader {
public:
    explicit TokenReader(std::span<const std::byte> bytes);

    [[nodiscard]] Result<Token> Next();
    [[nodiscard]] Result<Token> Peek();

    [[nodiscard]] Result<Token> Expect(TokenKind kind);
    [[nodiscard]] Result<std::int32_t> ReadInt();
    [[nodiscard]] Result<float> ReadFloat();
    [[nodiscard]] Result<std::string_view> ReadString();

    [[nodiscard]] Result<std::size_t> ReadListCount();

    /// Reads `[ count ] {` and returns the count.
    [[nodiscard]] Result<std::size_t> ReadListHeader();

private:
    static constexpr std::size_t kSequenceSlots = 16;
    static constexpr std::size_t kMaxSequenceLength = 16;

    struct Sequence {
        std::array<std::uint16_t, kMaxSequenceLength> codes{};
        std::uint8_t length = 0;
    };

    struct Expansion {
        Sequence sequence;
        std::uint8_t index = 0;
        std::uint16_t repeats = 0;
    };

    struct Code {
        std::uint16_t value = 0;
        std::size_t offset = 0;
    };

    Result<std::optional<Code>> NextCode();
    Code NextExpandedCode();
    Result<void> RunControl(std::uint16_t code);
    Result<std::uint16_t> ReadEscapedCode();
    Result<void> DefineSequence();
    Result<void> BeginRepeat();
    [[nodiscard]] Result<Sequence> FindSequence(std::uint16_t code) const;
    Result<Token> Decode(Code code);
    Result<Token> DecodeNumber(Code code);
    Result<Token> DecodeField(Code code);
    Result<Token> DecodeString(std::size_t offset);

    ByteReader bytes_;
    std::array<Sequence, kSequenceSlots> sequences_{};
    Expansion expansion_;
    std::optional<Token> peeked_;
};

}
