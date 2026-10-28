#include "assets/token_reader.hpp"

#include <bit>
#include <format>
#include <utility>

namespace rr::assets {
namespace {

constexpr std::uint16_t kString = 0x02;
constexpr std::uint16_t kFloat32 = 0x03;
constexpr std::uint16_t kInt32 = 0x04;
constexpr std::uint16_t kFirstDelimiter = 0x05;
constexpr std::uint16_t kUnsigned8 = 0x0B;
constexpr std::uint16_t kUnsigned8Alias = 0x0C;
constexpr std::uint16_t kSigned16 = 0x0D;
constexpr std::uint16_t kUnsigned16 = 0x0E;
constexpr std::uint16_t kFixed4096 = 0x0F;
constexpr std::uint16_t kFixed32 = 0x10;
constexpr std::uint16_t kWhole16 = 0x11;
constexpr std::uint16_t kUnit127 = 0x12;
constexpr std::uint16_t kExtendedField = 0x13;
constexpr std::uint16_t kRepeat = 0x14;
constexpr std::uint16_t kBracketMacro = 0x15;
constexpr std::uint16_t kDefineSequence = 0x16;
constexpr std::uint16_t kFirstSequence = 0x17;
constexpr std::uint16_t kLastSequence = 0x26;
constexpr std::uint16_t kFirstField = 0x27;
constexpr std::size_t kMaxStringLength = 63;

constexpr std::array kDelimiters{
    TokenKind::OpenBrace,    TokenKind::CloseBrace, TokenKind::OpenBracket,
    TokenKind::CloseBracket, TokenKind::Comma,      TokenKind::Semicolon,
};

bool IsSequence(std::uint16_t code) {
    return code >= kFirstSequence && code <= kLastSequence;
}

bool IsControl(std::uint16_t code) {
    return code >= kRepeat && code <= kLastSequence;
}

Token IntToken(std::size_t offset, std::int32_t value) {
    return {.kind = TokenKind::Int, .offset = offset, .integer = value};
}

Token FloatToken(std::size_t offset, float value) {
    return {.kind = TokenKind::Float, .offset = offset, .real = value};
}

}

std::string_view ToString(TokenKind kind) {
    switch (kind) {
        case TokenKind::End:
            return "end of data";
        case TokenKind::String:
            return "string";
        case TokenKind::Float:
            return "float";
        case TokenKind::Int:
            return "int";
        case TokenKind::OpenBrace:
            return "'{'";
        case TokenKind::CloseBrace:
            return "'}'";
        case TokenKind::OpenBracket:
            return "'['";
        case TokenKind::CloseBracket:
            return "']'";
        case TokenKind::Comma:
            return "','";
        case TokenKind::Semicolon:
            return "';'";
        case TokenKind::Field:
            return "field";
    }
    return "unknown token";
}

TokenReader::TokenReader(std::span<const std::byte> bytes) : bytes_(bytes, 0) {}

Result<Token> TokenReader::Next() {
    if (peeked_) {
        return *std::exchange(peeked_, std::nullopt);
    }
    Result<std::optional<Code>> code = NextCode();
    if (!code) {
        return std::unexpected(std::move(code.error()));
    }
    if (!code->has_value()) {
        return Token{.kind = TokenKind::End, .offset = bytes_.Position()};
    }
    return Decode(**code);
}

Result<Token> TokenReader::Peek() {
    if (!peeked_) {
        Result<Token> token = Next();
        if (!token) {
            return token;
        }
        peeked_ = *token;
    }
    return *peeked_;
}

Result<Token> TokenReader::Expect(TokenKind kind) {
    return Next().and_then([kind](const Token& token) -> Result<Token> {
        if (token.kind != kind) {
            return Fail(ErrorCode::InvalidData,
                        std::format("expected {} at offset {}, found {}", ToString(kind),
                                    token.offset, ToString(token.kind)));
        }
        return token;
    });
}

Result<std::int32_t> TokenReader::ReadInt() {
    return Expect(TokenKind::Int).transform([](const Token& token) { return token.integer; });
}

Result<float> TokenReader::ReadFloat() {
    return Expect(TokenKind::Float).transform([](const Token& token) { return token.real; });
}

Result<std::string_view> TokenReader::ReadString() {
    return Expect(TokenKind::String).transform([](const Token& token) { return token.text; });
}

Result<std::size_t> TokenReader::ReadListHeader() {
    Result<Token> open = Expect(TokenKind::OpenBracket);
    Result<std::int32_t> count = open.and_then([this](const Token&) { return ReadInt(); });
    if (!count) {
        return std::unexpected(std::move(count.error()));
    }
    if (*count < 0) {
        return Fail(ErrorCode::InvalidData,
                    std::format("negative list count {} after offset {}", *count, open->offset));
    }
    return Expect(TokenKind::CloseBracket)
        .and_then([this](const Token&) { return Expect(TokenKind::OpenBrace); })
        .transform([&count](const Token&) { return static_cast<std::size_t>(*count); });
}

Result<std::optional<TokenReader::Code>> TokenReader::NextCode() {
    while (expansion_.repeats == 0) {
        const std::size_t offset = bytes_.Position();
        if (bytes_.Remaining() == 0) {
            return std::nullopt;
        }
        Result<std::uint8_t> code = bytes_.ReadU8();
        if (!code) {
            return std::unexpected(std::move(code.error()));
        }
        if (!IsControl(*code)) {
            return Code{.value = *code, .offset = offset};
        }
        Result<void> ran = RunControl(*code);
        if (!ran) {
            return std::unexpected(std::move(ran.error()));
        }
    }
    return NextExpandedCode();
}

TokenReader::Code TokenReader::NextExpandedCode() {
    const Code code{
        .value = expansion_.sequence.codes.at(expansion_.index++),
        .offset = bytes_.Position(),
    };
    if (expansion_.index == expansion_.sequence.length) {
        expansion_.index = 0;
        --expansion_.repeats;
    }
    return code;
}

Result<void> TokenReader::RunControl(std::uint16_t code) {
    if (code == kRepeat) {
        return BeginRepeat();
    }
    if (code == kDefineSequence) {
        return DefineSequence();
    }
    if (code == kBracketMacro) {
        return Fail(ErrorCode::InvalidData,
                    std::format("unsupported bracket macro at offset {}", bytes_.Position() - 1));
    }
    return FindSequence(code).transform(
        [this](const Sequence& sequence) { expansion_ = {.sequence = sequence, .repeats = 1}; });
}

Result<std::uint16_t> TokenReader::ReadEscapedCode() {
    Result<std::uint8_t> code = bytes_.ReadU8();
    if (!code) {
        return std::unexpected(std::move(code.error()));
    }
    if (*code == kExtendedField) {
        return bytes_.ReadU16();
    }
    return *code;
}

Result<void> TokenReader::DefineSequence() {
    const std::size_t offset = bytes_.Position() - 1;
    Result<std::uint8_t> slot = bytes_.ReadU8();
    Result<std::uint8_t> length = slot.and_then([this](std::uint8_t) { return bytes_.ReadU8(); });
    if (!length) {
        return std::unexpected(std::move(length.error()));
    }
    if (!IsSequence(*slot) || *length == 0 || *length > kMaxSequenceLength) {
        return Fail(ErrorCode::InvalidData,
                    std::format("invalid sequence 0x{:02X} of length {} at offset {}", *slot,
                                *length, offset));
    }
    Sequence sequence{.length = *length};
    for (std::size_t i = 0; i < *length; ++i) {
        Result<std::uint16_t> code = ReadEscapedCode();
        if (!code) {
            return std::unexpected(std::move(code.error()));
        }
        if (IsControl(*code)) {
            return Fail(ErrorCode::InvalidData,
                        std::format("control code 0x{:02X} inside the sequence at offset {}", *code,
                                    offset));
        }
        sequence.codes.at(i) = *code;
    }
    sequences_.at(*slot - kFirstSequence) = sequence;
    return {};
}

Result<void> TokenReader::BeginRepeat() {
    const std::size_t offset = bytes_.Position() - 1;
    Result<std::uint16_t> count = bytes_.ReadU16();
    Result<std::uint16_t> code =
        count.and_then([this](std::uint16_t) { return ReadEscapedCode(); });
    if (!code) {
        return std::unexpected(std::move(code.error()));
    }
    if (IsSequence(*code)) {
        return FindSequence(*code).transform([this, &count](const Sequence& sequence) {
            expansion_ = {.sequence = sequence, .repeats = *count};
        });
    }
    if (IsControl(*code)) {
        return Fail(ErrorCode::InvalidData,
                    std::format("can't repeat control code 0x{:02X} at offset {}", *code, offset));
    }
    expansion_ = {.sequence = {.codes = {*code}, .length = 1}, .repeats = *count};
    return {};
}

Result<TokenReader::Sequence> TokenReader::FindSequence(std::uint16_t code) const {
    const Sequence& sequence = sequences_.at(code - kFirstSequence);
    if (sequence.length == 0) {
        return Fail(ErrorCode::InvalidData,
                    std::format("sequence 0x{:02X} is used before it's defined, near offset {}",
                                code, bytes_.Position()));
    }
    return sequence;
}

Result<Token> TokenReader::Decode(Code code) {
    if (code.value == kString) {
        return DecodeString(code.offset);
    }
    if (code.value >= kFirstDelimiter && code.value < kFirstDelimiter + kDelimiters.size()) {
        return Token{.kind = kDelimiters.at(code.value - kFirstDelimiter), .offset = code.offset};
    }
    if (code.value == kExtendedField || code.value >= kFirstField) {
        return DecodeField(code);
    }
    return DecodeNumber(code);
}

Result<Token> TokenReader::DecodeNumber(Code code) {
    const std::size_t offset = code.offset;
    const auto asInt = [offset](auto value) { return IntToken(offset, value); };
    const auto asScaled = [offset](float scale) {
        return [offset, scale](std::uint16_t value) {
            return FloatToken(offset,
                              static_cast<float>(std::bit_cast<std::int16_t>(value)) * scale);
        };
    };
    switch (code.value) {
        case kFloat32:
            return bytes_.ReadU32().transform([offset](std::uint32_t value) {
                return FloatToken(offset, std::bit_cast<float>(value));
            });
        case kInt32:
            return bytes_.ReadU32().transform(
                [&](std::uint32_t value) { return asInt(std::bit_cast<std::int32_t>(value)); });
        case kUnsigned8:
        case kUnsigned8Alias:
            return bytes_.ReadU8().transform(asInt);
        case kSigned16:
            return bytes_.ReadU16().transform(
                [&](std::uint16_t value) { return asInt(std::bit_cast<std::int16_t>(value)); });
        case kUnsigned16:
            return bytes_.ReadU16().transform(asInt);
        case kFixed4096:
            return bytes_.ReadU16().transform(asScaled(1.0F / 4096.0F));
        case kFixed32:
            return bytes_.ReadU16().transform(asScaled(1.0F / 32.0F));
        case kWhole16:
            return bytes_.ReadU16().transform(asScaled(1.0F));
        case kUnit127:
            return bytes_.ReadU8().transform([offset](std::uint8_t value) {
                return FloatToken(offset, static_cast<float>(value) * (1.0F / 127.0F));
            });
        default:
            return Fail(ErrorCode::InvalidData,
                        std::format("unknown token 0x{:02X} at offset {}", code.value, offset));
    }
}

Result<Token> TokenReader::DecodeField(Code code) {
    if (code.value != kExtendedField) {
        return Token{.kind = TokenKind::Field, .offset = code.offset, .field = code.value};
    }
    Result<std::uint16_t> field = bytes_.ReadU16();
    if (!field) {
        return std::unexpected(std::move(field.error()));
    }
    if (*field < kFirstField) {
        return Fail(ErrorCode::InvalidData,
                    std::format("extended field 0x{:04X} at offset {} isn't a field id", *field,
                                code.offset));
    }
    return Token{.kind = TokenKind::Field, .offset = code.offset, .field = *field};
}

Result<Token> TokenReader::DecodeString(std::size_t offset) {
    Result<std::span<const std::byte>> bytes = bytes_.ReadUntil(std::byte{0});
    if (!bytes) {
        return Fail(ErrorCode::InvalidData,
                    std::format("unterminated string at offset {}", offset));
    }
    if (bytes->size() > kMaxStringLength) {
        return Fail(
            ErrorCode::InvalidData,
            std::format("string at offset {} is longer than {} bytes", offset, kMaxStringLength));
    }
    const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    return Token{.kind = TokenKind::String, .offset = offset, .text = text};
}

}
