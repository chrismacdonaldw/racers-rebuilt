#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
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

void ApplyColorKey(ImageAsset& image, ColorKey key);

}
