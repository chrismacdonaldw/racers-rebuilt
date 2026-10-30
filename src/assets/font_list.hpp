#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/image_asset.hpp"
#include "assets/texture_database.hpp"
#include "core/result.hpp"

namespace rr::assets {

struct FontDefinition {
    std::string name;
    TextureFileKind fileKind = TextureFileKind::Tga;
    std::optional<ColorKey> colorKey;
    std::vector<char16_t> characters;
    std::int32_t spacingPx = 0;
    std::array<std::uint8_t, 3> textColor{0xFF, 0xFF, 0xFF};

    [[nodiscard]] std::string FileName() const { return ImageFileName(name, fileKind); }
};

[[nodiscard]] Result<std::vector<FontDefinition>> ParseFontList(std::span<const std::byte> bytes);

struct Glyph {
    char16_t character = 0;
    std::uint32_t leftPx = 0;
    std::uint32_t widthPx = 0;
};

/// `glyphs` is sorted by character.
struct FontGlyphs {
    std::uint32_t spaceWidthPx = 0;
    std::vector<Glyph> glyphs;
};

/// Characters without a glyph in the image are left out. Scan before applying the colour key.
[[nodiscard]] FontGlyphs ScanGlyphs(const ImageAsset& image, std::span<const char16_t> characters);

}
