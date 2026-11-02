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
constexpr FieldId kTextStyle = 0x33;
constexpr FieldId kCarouselStyle = 0x38;
constexpr FieldId kSelectorStyle = 0x39;
constexpr FieldId kGameButtonStyle = 0x3E;

constexpr std::array kImageStyleFields{
    Field<&ImageStyle::image>(0x28),
    Field<&ImageStyle::color>(0x2A),
};

constexpr std::array kTextStyleFields{
    Field<&TextStyle::font>(0x29),
    Field<&TextStyle::color>(0x2A),
    Field<&TextStyle::wrapWidth>(0x2E),
    Field<&TextStyle::centered>(0x2F),
};

constexpr std::array kIconStyleFields{
    Field<&IconStyle::sounds>(0x2B),
    Field<&IconStyle::stateRects>(0x2C),
    Field<&IconStyle::transitionMs>(0x2D),
    Field<&IconStyle::stateColors>(0x2A),
};

template <typename Style>
Result<void> ReadIconStyle(TokenReader& reader, Style& style) {
    return ReadBlock<IconStyle>(reader, kIconStyleFields, style.icon);
}

constexpr std::array kCarouselStyleFields{
    Field<&CarouselStyle::sounds>(0x2B),
    Field<&CarouselStyle::scrollMs>(0x2D),
    Field<&CarouselStyle::pageMode, &CarouselStyle::wraps>(0x2F),
};

constexpr std::array kSelectorStyleFields{
    FieldRule<SelectorStyle>{.id = 0x3A, .read = ReadIconStyle<SelectorStyle>},
    Field<&SelectorStyle::frame>(0x34),
    Field<&SelectorStyle::arrows>(0x35),
    Field<&SelectorStyle::itemColors>(0x2A),
};

constexpr std::array kGameButtonStyleFields{
    NestedField<&GameButtonStyle::button, &GameButtonElement::images>(0x28),
    NestedField<&GameButtonStyle::button, &GameButtonElement::fonts>(0x29),
    NestedField<&GameButtonStyle::button, &GameButtonElement::colors>(0x2A),
    Field<&GameButtonStyle::rects2C>(0x2C),
    Field<&GameButtonStyle::value2E>(0x2E),
    FieldRule<GameButtonStyle>{.id = 0x3A, .read = ReadIconStyle<GameButtonStyle>},
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
        case kTextStyle:
            return ReadStyles<TextStyle>(reader, kTextStyle, kTextStyleFields, styles.texts);
        case kCarouselStyle:
            return ReadStyles<CarouselStyle>(reader, kCarouselStyle, kCarouselStyleFields,
                                             styles.carousels);
        case kSelectorStyle:
            return ReadStyles<SelectorStyle>(reader, kSelectorStyle, kSelectorStyleFields,
                                             styles.selectors);
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

const TextStyle* MenuStyles::FindText(std::string_view name) const {
    return FindStyle<TextStyle>(texts, name);
}

const CarouselStyle* MenuStyles::FindCarousel(std::string_view name) const {
    return FindStyle<CarouselStyle>(carousels, name);
}

const SelectorStyle* MenuStyles::FindSelector(std::string_view name) const {
    return FindStyle<SelectorStyle>(selectors, name);
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
