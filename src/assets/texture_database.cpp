#include "assets/texture_database.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>
#include <utility>

#include "assets/bmp_image.hpp"
#include "assets/color_key_field.hpp"
#include "assets/png_image.hpp"
#include "assets/tga_image.hpp"
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

void FlipRows(ImageAsset& image) {
    const std::size_t rowBytes = std::size_t{image.widthPx} * kBytesPerPixel;
    const std::span<std::byte> pixels = image.rgba;
    for (std::size_t top = 0, bottom = image.heightPx; top + 1 < bottom; ++top, --bottom) {
        std::ranges::swap_ranges(pixels.subspan(top * rowBytes, rowBytes),
                                 pixels.subspan((bottom - 1) * rowBytes, rowBytes));
    }
}

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

Result<ImageAsset> DecodeImage(std::span<const std::byte> bytes, TextureFileKind kind) {
    if (IsPng(bytes)) {
        return DecodePng(bytes);
    }
    return kind == TextureFileKind::Bmp ? DecodeBmp(bytes) : DecodeTga(bytes);
}

Result<ImageAsset> DecodeTexture(std::span<const std::byte> bytes,
                                 const TextureDefinition& definition) {
    Result<ImageAsset> image = DecodeImage(bytes, definition.fileKind);
    if (image && definition.colorKey && !IsPng(bytes)) {
        ApplyColorKey(*image, *definition.colorKey);
    }
    if (image && definition.flipsVertically) {
        FlipRows(*image);
    }
    return image;
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
