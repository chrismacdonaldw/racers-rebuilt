#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/color.hpp"
#include "core/result.hpp"

namespace rr::assets {

enum class Shading : std::uint8_t {
    Flat,
    Gouraud,
};

enum class TextureMode : std::uint8_t {
    Decal,
    Modulate,
};

enum class TextureFilter : std::uint8_t {
    Linear,
    Point,
};

enum class TextureAddress : std::uint8_t {
    Wrap,
    Clamp,
};

enum class AlphaFunction : std::uint8_t {
    Always,
    Equal,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Never,
    NotEqual,
};

enum class BlendFactor : std::uint8_t {
    Zero,
    One,
    SourceColor,
    DestinationColor,
    InverseSourceColor,
    InverseDestinationColor,
    SourceAlpha,
    DestinationAlpha,
    InverseSourceAlpha,
    InverseDestinationAlpha,
    SourceAlphaSaturate,
};

struct AlphaTest {
    AlphaFunction function = AlphaFunction::Always;
    std::uint8_t reference = 0;

    friend constexpr bool operator==(const AlphaTest&, const AlphaTest&) = default;
};

struct BlendFactors {
    BlendFactor source = BlendFactor::One;
    BlendFactor destination = BlendFactor::Zero;

    friend constexpr bool operator==(const BlendFactors&, const BlendFactors&) = default;
};

struct MaterialAsset {
    std::string name;
    Rgba8 ambient = kOpaqueWhite;
    Rgba8 diffuse = kOpaqueWhite;
    Shading shading = Shading::Gouraud;
    std::string texture;
    TextureMode textureMode = TextureMode::Decal;
    TextureFilter filter = TextureFilter::Linear;
    TextureAddress address = TextureAddress::Wrap;
    std::optional<AlphaTest> alphaTest;
    std::optional<BlendFactors> blend;
    std::optional<std::uint8_t> transparency;
    bool rotatesVertices = false;
    bool hasFlag4A = false;
    bool hasFlag4B = false;
    bool hasFlag4C = false;
};

[[nodiscard]] Result<std::vector<MaterialAsset>> ParseMaterialLibrary(
    std::span<const std::byte> bytes);

}
