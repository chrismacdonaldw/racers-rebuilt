#include "assets/material_library.hpp"

#include <array>
#include <format>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kMaterialField = 0x27;
constexpr FieldId kFirstAlphaFunction = 0x30;
constexpr FieldId kFirstBlendFactor = 0x39;

template <typename Enum>
Result<Enum> ReadEnumField(TokenReader& reader, FieldId first, Enum last) {
    return reader.Expect(TokenKind::Field).and_then([&](const Token& field) -> Result<Enum> {
        if (field.field < first || field.field - first > std::to_underlying(last)) {
            return Fail(
                ErrorCode::InvalidData,
                std::format("unexpected field 0x{:02X} at offset {}", field.field, field.offset));
        }
        return static_cast<Enum>(field.field - first);
    });
}

Result<void> ReadAlphaTest(TokenReader& reader, MaterialAsset& material) {
    AlphaTest& test = material.alphaTest.emplace();
    return ReadEnumField(reader, kFirstAlphaFunction, AlphaFunction::NotEqual)
        .and_then([&](AlphaFunction function) -> Result<void> {
            test.function = function;
            if (function == AlphaFunction::Always || function == AlphaFunction::Never) {
                return {};
            }
            return ReadValue(reader, test.reference);
        });
}

Result<void> ReadBlend(TokenReader& reader, MaterialAsset& material) {
    BlendFactors& blend = material.blend.emplace();
    for (BlendFactor* factor : {&blend.source, &blend.destination}) {
        Result<BlendFactor> read =
            ReadEnumField(reader, kFirstBlendFactor, BlendFactor::SourceAlphaSaturate);
        if (!read) {
            return std::unexpected(std::move(read.error()));
        }
        *factor = *read;
    }
    return {};
}

constexpr std::array kMaterialFields{
    Field<&MaterialAsset::ambient>(0x28),
    Field<&MaterialAsset::diffuse>(0x29),
    FlagField<&MaterialAsset::shading, Shading::Flat>(0x2A),
    FlagField<&MaterialAsset::shading, Shading::Gouraud>(0x2B),
    Field<&MaterialAsset::texture>(0x2C),
    FlagField<&MaterialAsset::textureMode, TextureMode::Modulate>(0x2D),
    FlagField<&MaterialAsset::textureMode, TextureMode::Decal>(0x2E),
    FieldRule<MaterialAsset>{.id = 0x2F, .read = ReadAlphaTest},
    FieldRule<MaterialAsset>{.id = 0x38, .read = ReadBlend},
    FlagField<&MaterialAsset::filter, TextureFilter::Linear>(0x44),
    FlagField<&MaterialAsset::filter, TextureFilter::Point>(0x45),
    Field<&MaterialAsset::transparency>(0x46),
    Field<&MaterialAsset::rotatesVertices>(0x47),
    FlagField<&MaterialAsset::address, TextureAddress::Wrap>(0x48),
    FlagField<&MaterialAsset::address, TextureAddress::Clamp>(0x49),
    Field<&MaterialAsset::hasFlag4A>(0x4A),
    Field<&MaterialAsset::hasFlag4B>(0x4B),
    Field<&MaterialAsset::hasFlag4C>(0x4C),
    Field<&MaterialAsset::transparency>(0x4D),
    Field<&MaterialAsset::transparency>(0x4E),
    Field<&MaterialAsset::transparency>(0x4F),
    Field<&MaterialAsset::transparency>(0x50),
};

}

Result<std::vector<MaterialAsset>> ParseMaterialLibrary(std::span<const std::byte> bytes) {
    return ParseNamedLibrary<MaterialAsset>(bytes, kMaterialField, kMaterialFields);
}

}
