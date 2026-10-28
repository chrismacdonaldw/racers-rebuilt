#include "assets/material_library.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Join;

Result<std::vector<MaterialAsset>> ParseOneMaterial(const std::vector<std::byte>& fields) {
    return ParseMaterialLibrary(Join({
        Bytes({0x27, 0x07, 0x0B, 0x01, 0x08, 0x05}),
        Bytes({0x27, 0x02, 'm', 0x00, 0x05}),
        fields,
        Bytes({0x06, 0x06}),
    }));
}

TEST_CASE("material fields set every property") {
    const Result<std::vector<MaterialAsset>> materials = ParseOneMaterial(Join({
        Bytes({0x28, 0x0B, 0x01, 0x0B, 0x02, 0x0B, 0x03, 0x0B, 0x04}),
        Bytes({0x29, 0x0B, 0x05, 0x0B, 0x06, 0x0B, 0x07, 0x0B, 0x08}),
        Bytes({0x2A, 0x2C, 0x02, 'w', 'o', 'o', 'd', 0x00, 0x2D}),
        Bytes({0x2F, 0x32, 0x0B, 0x80, 0x38, 0x3F, 0x41}),
        Bytes({0x45, 0x46, 0x0B, 0xC8, 0x47, 0x49, 0x4A, 0x4B, 0x4C}),
    }));

    REQUIRE(materials.has_value());
    REQUIRE(materials->size() == 1);
    const MaterialAsset& material = materials->front();
    CHECK(material.name == "m");
    CHECK(material.ambient == Rgba8{1, 2, 3, 4});
    CHECK(material.diffuse == Rgba8{5, 6, 7, 8});
    CHECK(material.shading == Shading::Flat);
    CHECK(material.texture == "wood");
    CHECK(material.textureMode == TextureMode::Modulate);
    CHECK(material.alphaTest == AlphaTest{.function = AlphaFunction::Greater, .reference = 0x80});
    CHECK(material.blend == BlendFactors{.source = BlendFactor::SourceAlpha,
                                         .destination = BlendFactor::InverseSourceAlpha});
    CHECK(material.filter == TextureFilter::Point);
    CHECK(material.transparency == std::uint8_t{200});
    CHECK(material.rotatesVertices);
    CHECK(material.address == TextureAddress::Clamp);
    CHECK(material.hasFlag4A);
    CHECK(material.hasFlag4B);
    CHECK(material.hasFlag4C);
}

TEST_CASE("a material without fields keeps the defaults") {
    const Result<std::vector<MaterialAsset>> materials = ParseOneMaterial({});

    REQUIRE(materials.has_value());
    const MaterialAsset& material = materials->front();
    CHECK(material.ambient == kOpaqueWhite);
    CHECK(material.diffuse == kOpaqueWhite);
    CHECK(material.shading == Shading::Gouraud);
    CHECK(material.texture.empty());
    CHECK(material.textureMode == TextureMode::Decal);
    CHECK(material.filter == TextureFilter::Linear);
    CHECK(material.address == TextureAddress::Wrap);
    CHECK_FALSE(material.alphaTest.has_value());
    CHECK_FALSE(material.blend.has_value());
    CHECK_FALSE(material.transparency.has_value());
    CHECK_FALSE(material.rotatesVertices);
}

TEST_CASE("always and never alpha tests take no reference") {
    const Result<std::vector<MaterialAsset>> always = ParseOneMaterial(Bytes({0x2F, 0x30, 0x45}));
    const Result<std::vector<MaterialAsset>> never = ParseOneMaterial(Bytes({0x2F, 0x36, 0x45}));

    REQUIRE(always.has_value());
    REQUIRE(never.has_value());
    CHECK(always->front().alphaTest == AlphaTest{.function = AlphaFunction::Always});
    CHECK(never->front().alphaTest == AlphaTest{.function = AlphaFunction::Never});
    CHECK(never->front().filter == TextureFilter::Point);
}

TEST_CASE("the later transparency fields set the same value") {
    const Result<std::vector<MaterialAsset>> materials =
        ParseOneMaterial(Bytes({0x46, 0x0B, 0x01, 0x50, 0x0B, 0x02}));

    REQUIRE(materials.has_value());
    CHECK(materials->front().transparency == std::uint8_t{2});
}

TEST_CASE("an unknown alpha function or blend factor fails") {
    CHECK_FALSE(ParseOneMaterial(Bytes({0x2F, 0x38})).has_value());
    CHECK_FALSE(ParseOneMaterial(Bytes({0x38, 0x3F, 0x44})).has_value());
    CHECK_FALSE(ParseOneMaterial(Bytes({0x38, 0x0B, 0x01, 0x0B, 0x02})).has_value());
}

TEST_CASE("material values must fit in a byte") {
    CHECK_FALSE(ParseOneMaterial(Bytes({0x46, 0x0E, 0x00, 0x01})).has_value());
    CHECK_FALSE(
        ParseOneMaterial(Bytes({0x28, 0x0B, 0x00, 0x0B, 0x00, 0x0B, 0x00, 0x0D, 0xFF, 0xFF}))
            .has_value());
}

TEST_CASE("an unknown material field fails") {
    CHECK_FALSE(ParseOneMaterial(Bytes({0x51})).has_value());
}

TEST_CASE("a library must start with its material list") {
    CHECK_FALSE(
        ParseMaterialLibrary(Bytes({0x28, 0x07, 0x0B, 0x00, 0x08, 0x05, 0x06})).has_value());
    CHECK(ParseMaterialLibrary(Bytes({0x27, 0x07, 0x0B, 0x00, 0x08, 0x05, 0x06})).has_value());
}

}
}
