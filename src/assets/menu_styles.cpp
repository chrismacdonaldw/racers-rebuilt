#include "assets/menu_styles.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/menu_fields.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kCapacityField = 0x27;
constexpr FieldId kImageStyle = 0x32;
constexpr FieldId kGameButtonStyle = 0x3E;

constexpr std::array kImageStyleFields{
    Field<&ImageStyle::image>(0x28),
    Field<&ImageStyle::color>(0x2A),
};

Result<void> SkipControlFields(TokenReader& reader, [[maybe_unused]] GameButtonStyle& style) {
    return SkipBlock(reader);
}

constexpr std::array kGameButtonStyleFields{
    NestedField<&GameButtonStyle::button, &GameButtonElement::images>(0x28),
    NestedField<&GameButtonStyle::button, &GameButtonElement::fonts>(0x29),
    NestedField<&GameButtonStyle::button, &GameButtonElement::colors>(0x2A),
    Field<&GameButtonStyle::rects2C>(0x2C),
    Field<&GameButtonStyle::value2E>(0x2E),
    FieldRule<GameButtonStyle>{.id = 0x3A, .read = SkipControlFields},
};

Result<void> ReadEntryName(TokenReader& reader, FieldId tag, std::string& name) {
    return ExpectField(reader, tag)
        .and_then([&] { return reader.ReadString(); })
        .transform([&](std::string_view read) { name = read; });
}

template <typename Style>
Result<void> ReadStyles(TokenReader& reader, FieldId tag, std::span<const FieldRule<Style>> rules,
                        std::vector<Style>& styles) {
    return ReadList(reader, [&] {
        Style& style = styles.emplace_back();
        return ReadEntryName(reader, tag, style.name).and_then([&] {
            return ReadBlock<Style>(reader, rules, style);
        });
    });
}

Result<void> SkipStyles(TokenReader& reader) {
    return ReadList(reader, [&] {
        return reader.Expect(TokenKind::Field)
            .and_then([&](const Token&) { return reader.ReadString(); })
            .and_then([&](std::string_view) { return SkipBlock(reader); });
    });
}

Result<void> ReadSection(TokenReader& reader, MenuStyles& styles) {
    Result<Token> tag = reader.Expect(TokenKind::Field);
    if (!tag) {
        return std::unexpected(std::move(tag.error()));
    }
    switch (tag->field) {
        case kImageStyle:
            return ReadStyles<ImageStyle>(reader, kImageStyle, kImageStyleFields, styles.images);
        case kGameButtonStyle:
            return ReadStyles<GameButtonStyle>(reader, kGameButtonStyle, kGameButtonStyleFields,
                                               styles.gameButtons);
        default:
            return SkipStyles(reader);
    }
}

template <typename Style>
const Style* FindStyle(std::span<const Style> styles, std::string_view name) {
    const auto style = std::ranges::find(styles, name, &Style::name);
    return style == styles.end() ? nullptr : &*style;
}

}

const ImageStyle* MenuStyles::FindImage(std::string_view name) const {
    return FindStyle<ImageStyle>(images, name);
}

const GameButtonStyle* MenuStyles::FindGameButton(std::string_view name) const {
    return FindStyle<GameButtonStyle>(gameButtons, name);
}

Result<MenuStyles> ParseMenuStyles(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    MenuStyles styles;
    Result<void> header = ExpectField(reader, kCapacityField).and_then([&] {
        return reader.ReadInt().transform([](std::int32_t) {});
    });
    if (!header) {
        return std::unexpected(std::move(header.error()));
    }
    while (true) {
        Result<Token> next = reader.Peek();
        if (!next) {
            return std::unexpected(std::move(next.error()));
        }
        if (next->kind == TokenKind::End) {
            return styles;
        }
        Result<void> section = ReadSection(reader, styles);
        if (!section) {
            return std::unexpected(std::move(section.error()));
        }
    }
}

}
