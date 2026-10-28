#include "assets/texture_database.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kTextureField = 0x27;
constexpr std::size_t kBytesPerPixel = 4;
constexpr std::size_t kAlphaOffset = 3;

template <TextureFileKind Kind>
Result<void> SetFileKind([[maybe_unused]] TokenReader& reader, TextureDefinition& texture) {
    texture.fileKind = Kind;
    return {};
}

Result<std::uint8_t> ReadChannel(TokenReader& reader) {
    return reader.ReadInt().and_then([](std::int32_t value) -> Result<std::uint8_t> {
        if (!std::in_range<std::uint8_t>(value)) {
            return Fail(ErrorCode::InvalidData,
                        std::format("color key channel {} is outside 0 to 255", value));
        }
        return static_cast<std::uint8_t>(value);
    });
}

Result<void> ReadColorKey(TokenReader& reader, TextureDefinition& texture) {
    ColorKey key;
    for (std::uint8_t* channel : {&key.red, &key.green, &key.blue}) {
        Result<std::uint8_t> value = ReadChannel(reader);
        if (!value) {
            return std::unexpected(std::move(value.error()));
        }
        *channel = *value;
    }
    texture.colorKey = key;
    return {};
}

constexpr std::array kTextureFields{
    Field<&TextureDefinition::flipsVertically>(0x28),
    Field<&TextureDefinition::mipmapCount>(0x29),
    FieldRule<TextureDefinition>{.id = 0x2A, .read = SetFileKind<TextureFileKind::Bmp>},
    FieldRule<TextureDefinition>{.id = 0x2B, .read = SetFileKind<TextureFileKind::Tga>},
    FieldRule<TextureDefinition>{.id = 0x2C, .read = ReadColorKey},
    Field<&TextureDefinition::hasFlag2D>(0x2D),
    Field<&TextureDefinition::hasFlag2E>(0x2E),
};

Result<void> ReadTextures(TokenReader& reader, std::vector<TextureDefinition>& textures) {
    return ReadList(reader, [&] {
        TextureDefinition& texture = textures.emplace_back();
        return reader.Expect(TokenKind::Field)
            .and_then([&](const Token& field) -> Result<std::string_view> {
                if (field.field != kTextureField) {
                    return Fail(ErrorCode::InvalidData,
                                std::format("expected a texture at offset {}", field.offset));
                }
                return reader.ReadString();
            })
            .and_then([&](std::string_view name) {
                texture.name = name;
                return ReadBlock<TextureDefinition>(reader, kTextureFields, texture);
            });
    });
}

constexpr std::array kDatabaseFields{
    FieldRule<std::vector<TextureDefinition>>{.id = kTextureField, .read = ReadTextures},
};

}

std::string TextureDefinition::FileName() const {
    return std::format("{}.{}", name, fileKind == TextureFileKind::Bmp ? "BMP" : "TGA");
}

Result<std::vector<TextureDefinition>> ParseTextureDatabase(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    std::vector<TextureDefinition> textures;
    return ReadFields<std::vector<TextureDefinition>>(reader, kDatabaseFields, textures)
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .transform([&](const Token&) { return std::move(textures); });
}

void ApplyColorKey(ImageAsset& image, ColorKey key) {
    const std::array keyBytes{std::byte{key.red}, std::byte{key.green}, std::byte{key.blue}};
    for (std::size_t i = 0; i + kBytesPerPixel <= image.rgba.size(); i += kBytesPerPixel) {
        const std::span<std::byte> pixel = std::span(image.rgba).subspan(i, kBytesPerPixel);
        if (std::ranges::equal(pixel.first(kAlphaOffset), keyBytes)) {
            pixel[kAlphaOffset] = std::byte{0};
        }
    }
}

}
