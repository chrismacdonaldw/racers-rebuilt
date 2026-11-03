#include "assets/texture_database.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

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
constexpr std::int32_t kMaxGameMipLevels = 4;

constexpr std::array kTextureFields{
    Field<&TextureDefinition::flipsVertically>(0x28),
    Field<&TextureDefinition::mipmapCount>(0x29),
    FlagField<&TextureDefinition::fileKind, TextureFileKind::Bmp>(0x2A),
    FlagField<&TextureDefinition::fileKind, TextureFileKind::Tga>(0x2B),
    Field<&TextureDefinition::colorKey>(0x2C),
    Field<&TextureDefinition::hasFlag2D>(0x2D),
    Field<&TextureDefinition::hasFlag2E>(0x2E),
};

using Rgb = std::array<std::byte, kAlphaOffset>;

class ColourSum {
public:
    void Add(std::span<const std::byte> pixel) {
        for (std::size_t channel = 0; channel < kAlphaOffset; ++channel) {
            totals_[channel] += std::to_integer<std::uint32_t>(pixel[channel]);
        }
        ++count_;
    }

    [[nodiscard]] std::optional<Rgb> Mean() const {
        if (count_ == 0) {
            return std::nullopt;
        }
        Rgb mean{};
        for (std::size_t channel = 0; channel < kAlphaOffset; ++channel) {
            mean[channel] = static_cast<std::byte>((totals_[channel] + (count_ / 2)) / count_);
        }
        return mean;
    }

private:
    std::array<std::uint32_t, kAlphaOffset> totals_{};
    std::uint32_t count_ = 0;
};

std::span<std::byte> PixelAt(ImageAsset& image, std::size_t index) {
    return std::span(image.rgba).subspan(index * kBytesPerPixel, kBytesPerPixel);
}

std::span<const std::byte> PixelAt(const ImageAsset& image, std::size_t index) {
    return std::span(image.rgba).subspan(index * kBytesPerPixel, kBytesPerPixel);
}

std::optional<Rgb> NeighbourMean(const ImageAsset& image, const std::vector<bool>& keyed,
                                 std::size_t index) {
    const std::size_t width = image.widthPx;
    const std::size_t x = index % width;
    const std::size_t y = index / width;
    const std::array<bool, 4> isInside{x > 0, x + 1 < width, y > 0, y + 1 < image.heightPx};
    const std::array<std::size_t, 4> neighbours{index - 1, index + 1, index - width, index + width};
    ColourSum sum;
    for (std::size_t i = 0; i < neighbours.size(); ++i) {
        if (isInside.at(i) && !keyed[neighbours.at(i)]) {
            sum.Add(PixelAt(image, neighbours.at(i)));
        }
    }
    return sum.Mean();
}

std::uint32_t GameMipLevels(const ImageAsset& image, const TextureDefinition& definition) {
    if (definition.mipmapCount <= 1 || image.widthPx != image.heightPx) {
        return 1;
    }
    const auto requested =
        static_cast<std::uint32_t>(std::min(definition.mipmapCount, kMaxGameMipLevels));
    return std::min(requested, FullMipLevels(image.widthPx, image.heightPx));
}

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
    if (image && !IsPng(bytes)) {
        image->mipLevels = GameMipLevels(*image, definition);
    }
    return image;
}

void ApplyColorKey(ImageAsset& image, ColorKey key) {
    const std::array keyBytes{std::byte{key.red}, std::byte{key.green}, std::byte{key.blue}};
    const std::size_t pixelCount = image.rgba.size() / kBytesPerPixel;
    std::vector<bool> keyed(pixelCount);
    ColourSum unkeyed;
    for (std::size_t i = 0; i < pixelCount; ++i) {
        const std::span<std::byte> pixel = PixelAt(image, i);
        keyed[i] = std::ranges::equal(pixel.first(kAlphaOffset), keyBytes);
        if (keyed[i]) {
            pixel[kAlphaOffset] = std::byte{0};
        } else {
            unkeyed.Add(pixel);
        }
    }
    const std::optional<Rgb> mean = unkeyed.Mean();
    if (!mean) {
        return;
    }
    for (std::size_t i = 0; i < pixelCount; ++i) {
        if (keyed[i]) {
            std::ranges::copy(NeighbourMean(image, keyed, i).value_or(*mean),
                              PixelAt(image, i).begin());
        }
    }
}

}
