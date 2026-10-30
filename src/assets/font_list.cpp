#include "assets/font_list.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <utility>

#include "assets/color_key_field.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kFontField = 0x27;
constexpr std::size_t kMaxCharacters = 100;
constexpr std::size_t kBytesPerPixel = 4;

Result<char16_t> ToCharacter(const Token& token) {
    if (token.kind == TokenKind::String && token.text.size() == 1) {
        return static_cast<char16_t>(static_cast<unsigned char>(token.text.front()));
    }
    if (token.kind == TokenKind::Int && std::in_range<std::uint16_t>(token.integer)) {
        return static_cast<char16_t>(token.integer);
    }
    return Fail(ErrorCode::InvalidData,
                std::format("font character at offset {} isn't one character or a code from 0 to "
                            "0xFFFF",
                            token.offset));
}

Result<void> ReadCharacters(TokenReader& reader, FontDefinition& font) {
    Result<Token> open = reader.Expect(TokenKind::OpenBracket);
    if (!open) {
        return std::unexpected(std::move(open.error()));
    }
    font.characters.clear();
    while (true) {
        Result<Token> token = reader.Next();
        if (!token) {
            return std::unexpected(std::move(token.error()));
        }
        if (token->kind == TokenKind::CloseBracket) {
            return {};
        }
        Result<char16_t> character = ToCharacter(*token);
        if (!character) {
            return std::unexpected(std::move(character.error()));
        }
        if (font.characters.size() == kMaxCharacters) {
            return Fail(ErrorCode::InvalidData,
                        std::format("font {} lists over {} characters", font.name, kMaxCharacters));
        }
        font.characters.push_back(*character);
    }
}

constexpr std::array kFontFields{
    FlagField<&FontDefinition::fileKind, TextureFileKind::Bmp>(0x28),
    FlagField<&FontDefinition::fileKind, TextureFileKind::Tga>(0x29),
    Field<&FontDefinition::colorKey>(0x2A),
    FieldRule<FontDefinition>{.id = 0x2B, .read = ReadCharacters},
    Field<&FontDefinition::spacingPx>(0x2C),
    Field<&FontDefinition::textColor>(0x2D),
};

class SeparatorColumns {
public:
    explicit SeparatorColumns(const ImageAsset& image) : image_(image) {}

    [[nodiscard]] bool IsSeparator(std::uint32_t x) const {
        return std::ranges::all_of(std::views::iota(0U, image_.heightPx), [&](std::uint32_t y) {
            return std::ranges::equal(Pixel(x, y), Pixel(0, y));
        });
    }

private:
    [[nodiscard]] std::span<const std::byte> Pixel(std::uint32_t x, std::uint32_t y) const {
        const std::size_t index = (static_cast<std::size_t>(y) * image_.widthPx) + x;
        return std::span(image_.rgba).subspan(index * kBytesPerPixel, kBytesPerPixel);
    }

    const ImageAsset& image_;
};

}

Result<std::vector<FontDefinition>> ParseFontList(std::span<const std::byte> bytes) {
    return ParseNamedLibrary<FontDefinition>(bytes, kFontField, kFontFields);
}

FontGlyphs ScanGlyphs(const ImageAsset& image, std::span<const char16_t> characters) {
    const SeparatorColumns columns(image);
    const std::uint32_t width = image.widthPx;
    FontGlyphs font;
    while (font.spaceWidthPx < width && columns.IsSeparator(font.spaceWidthPx)) {
        ++font.spaceWidthPx;
    }
    std::uint32_t x = font.spaceWidthPx + 1;
    for (const char16_t character : characters) {
        if (!font.glyphs.empty()) {
            while (x < width && columns.IsSeparator(x)) {
                ++x;
            }
        }
        const std::uint32_t left = x;
        while (x < width && !columns.IsSeparator(x)) {
            ++x;
        }
        if (x <= left) {
            break;
        }
        font.glyphs.push_back({.character = character, .leftPx = left, .widthPx = x - left});
    }
    std::ranges::stable_sort(font.glyphs, {}, &Glyph::character);
    return font;
}

}
