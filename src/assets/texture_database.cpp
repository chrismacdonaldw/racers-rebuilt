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

Result<void> ReadColorKey(TokenReader& reader, TextureDefinition& texture) {
    ColorKey& key = texture.colorKey.emplace();
    for (std::uint8_t* channel : {&key.red, &key.green, &key.blue}) {
        Result<void> read = ReadValue(reader, *channel);
        if (!read) {
            return read;
        }
    }
    return {};
}

constexpr std::array kTextureFields{
    Field<&TextureDefinition::flipsVertically>(0x28),
    Field<&TextureDefinition::mipmapCount>(0x29),
    FlagField<&TextureDefinition::fileKind, TextureFileKind::Bmp>(0x2A),
    FlagField<&TextureDefinition::fileKind, TextureFileKind::Tga>(0x2B),
    FieldRule<TextureDefinition>{.id = 0x2C, .read = ReadColorKey},
    Field<&TextureDefinition::hasFlag2D>(0x2D),
    Field<&TextureDefinition::hasFlag2E>(0x2E),
};

}

std::string TextureDefinition::FileName() const {
    return std::format("{}.{}", name, fileKind == TextureFileKind::Bmp ? "BMP" : "TGA");
}

Result<std::vector<TextureDefinition>> ParseTextureDatabase(std::span<const std::byte> bytes) {
    return ParseNamedLibrary<TextureDefinition>(bytes, kTextureField, kTextureFields);
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
