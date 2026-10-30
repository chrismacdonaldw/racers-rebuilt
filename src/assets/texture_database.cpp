#include "assets/texture_database.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>
#include <utility>

#include "assets/color_key_field.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kTextureField = 0x27;
constexpr std::size_t kBytesPerPixel = 4;
constexpr std::size_t kAlphaOffset = 3;

constexpr std::array kTextureFields{
    Field<&TextureDefinition::flipsVertically>(0x28),
    Field<&TextureDefinition::mipmapCount>(0x29),
    FlagField<&TextureDefinition::fileKind, TextureFileKind::Bmp>(0x2A),
    FlagField<&TextureDefinition::fileKind, TextureFileKind::Tga>(0x2B),
    Field<&TextureDefinition::colorKey>(0x2C),
    Field<&TextureDefinition::hasFlag2D>(0x2D),
    Field<&TextureDefinition::hasFlag2E>(0x2E),
};

}

Result<void> ReadValue(TokenReader& reader, ColorKey& key) {
    return ReadMembers<&ColorKey::red, &ColorKey::green, &ColorKey::blue>(reader, key);
}

std::string ImageFileName(std::string_view name, TextureFileKind kind) {
    return std::format("{}.{}", name, kind == TextureFileKind::Bmp ? "BMP" : "TGA");
}

std::string TextureDefinition::FileName() const {
    return ImageFileName(name, fileKind);
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
