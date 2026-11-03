#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/image_asset.hpp"
#include "core/result.hpp"

namespace rr::assets {

enum class TextureFileKind : std::uint8_t {
    Tga,
    Bmp,
};

struct ColorKey {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    friend constexpr bool operator==(const ColorKey&, const ColorKey&) = default;
};

[[nodiscard]] std::string ImageFileName(std::string_view name, TextureFileKind kind);

struct TextureDefinition {
    std::string name;
    TextureFileKind fileKind = TextureFileKind::Tga;
    bool flipsVertically = false;
    std::int32_t mipmapCount = 0;
    std::optional<ColorKey> colorKey;
    bool hasFlag2D = false;
    bool hasFlag2E = false;

    [[nodiscard]] std::string FileName() const;
};

[[nodiscard]] Result<std::vector<TextureDefinition>> ParseTextureDatabase(
    std::span<const std::byte> bytes);

/// Decodes PNG bytes as PNG, whatever `kind` says.
[[nodiscard]] Result<ImageAsset> DecodeImage(std::span<const std::byte> bytes,
                                             TextureFileKind kind);

/// Keys out the definition's colour, unless the bytes are PNG, and flips the rows when it asks.
[[nodiscard]] Result<ImageAsset> DecodeTexture(std::span<const std::byte> bytes,
                                               const TextureDefinition& definition);

/// Keyed pixels turn transparent; `enhancedRgba` also takes their neighbours' colour.
void ApplyColorKey(ImageAsset& image, ColorKey key);

}
