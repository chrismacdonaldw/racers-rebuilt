#include "assets/ldraw_prints.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <utility>

#include "archive_survey.hpp"
#include "assets/ldraw_part.hpp"
#include "image_files.hpp"

namespace rr::assets {
namespace {

constexpr std::uint32_t kSizePx = 64;
constexpr Rgba8 kMagenta{0xFF, 0x00, 0xFF, 0xFF};
constexpr Rgba8 kGreen{0x00, 0xFF, 0x00, 0xFF};
constexpr Rgba8 kRed{0xB4, 0x00, 0x00, 0xFF};
constexpr Rgba8 kWhite{0xF4, 0xF4, 0xF4, 0xFF};

struct Square {
    float left = -1.0F;
    float right = 1.0F;
    float top = 0.0F;
    float bottom = -1.6F;
    float depth = -0.5F;
    std::string colour = "15";
};

void AddSquare(GeometryAsset& part, const Square& square) {
    const auto first = static_cast<std::uint32_t>(part.vertices.size());
    const auto firstIndex = static_cast<std::uint32_t>(part.indices.size());
    for (const auto& [x, z] : {std::pair{square.left, square.top},
                               {square.left, square.bottom},
                               {square.right, square.bottom},
                               {square.right, square.top}}) {
        part.vertices.push_back({.position = {.x = x, .y = square.depth, .z = z}});
    }
    part.indices.insert(part.indices.end(),
                        {first, first + 1, first + 2, first, first + 2, first + 3});
    part.ranges.push_back({.firstIndex = firstIndex,
                           .indexCount = 6,
                           .material = FindOrAddMaterial(part.materials, square.colour)});
}

ImageAsset TorsoWithSide(const Rgba8& side) {
    ImageAsset torso = test::SolidImage(8, 8, std::byte{0xFF});
    const std::size_t at = ((4 * std::size_t{8}) + 0) * 4;
    for (std::size_t channel = 0; channel < side.size(); ++channel) {
        torso.rgba[at + channel] = static_cast<std::byte>(side.at(channel));
    }
    return torso;
}

Rgba8 PanelPixel(const ImageAsset& image, float across, float down) {
    const float v = 0.023F + (down * 0.954F);
    const float left = std::lerp(0.199F, 0.095F, down);
    const float right = std::lerp(0.89F, 1.0F, down);
    const float u = left + (across * (right - left));
    const auto x = static_cast<std::size_t>(u * static_cast<float>(image.widthPx));
    const auto y = static_cast<std::size_t>(v * static_cast<float>(image.heightPx));
    const std::size_t at = ((y * image.widthPx) + x) * 4;
    return {std::to_integer<std::uint8_t>(image.rgba[at]),
            std::to_integer<std::uint8_t>(image.rgba[at + 1]),
            std::to_integer<std::uint8_t>(image.rgba[at + 2]),
            std::to_integer<std::uint8_t>(image.rgba[at + 3])};
}

Rgba8 CornerPixel(const ImageAsset& image) {
    return {
        std::to_integer<std::uint8_t>(image.rgba[0]), std::to_integer<std::uint8_t>(image.rgba[1]),
        std::to_integer<std::uint8_t>(image.rgba[2]), std::to_integer<std::uint8_t>(image.rgba[3])};
}

TEST_CASE("a torso print draws the part's front over the front panel and keeps the rest") {
    GeometryAsset part;
    AddSquare(part, {});
    AddSquare(part, {.left = 0.0F, .bottom = -0.8F, .depth = -0.6F, .colour = "4"});

    const ImageAsset image = DrawTorsoPrint(TorsoWithSide(kGreen), part, kSizePx);

    REQUIRE(image.widthPx == kSizePx);
    REQUIRE(image.heightPx == kSizePx);
    CHECK(PanelPixel(image, 0.75F, 0.25F) == kRed);
    CHECK(PanelPixel(image, 0.25F, 0.25F) == kWhite);
    CHECK(PanelPixel(image, 0.75F, 0.75F) == kWhite);
    CHECK(CornerPixel(image) == kMagenta);
}

TEST_CASE("a torso print's main colour is the torso's side colour, and back faces are left out") {
    GeometryAsset part;
    AddSquare(part, {.colour = "16"});
    AddSquare(part, {.left = 1.0F, .right = -1.0F, .depth = -0.7F, .colour = "4"});

    const ImageAsset image = DrawTorsoPrint(TorsoWithSide(kGreen), part, kSizePx);

    CHECK(PanelPixel(image, 0.5F, 0.5F) == kGreen);
}

TEST_CASE("a torso's print is found by name in any case") {
    CHECK(LdrawPrintFor("in_chst").transform([](const LdrawPrint& print) { return print.file; }) ==
          "973pz4.dat");
    CHECK_FALSE(LdrawPrintFor("RR_CHST").has_value());
}

std::string UnknownColours(LdrawLibrary& library, const LdrawPrint& print) {
    const Result<GeometryAsset> part = library.LoadPart(print.file);
    if (!part) {
        return part.error().message;
    }
    std::string unknown;
    for (const std::string& material : part->materials) {
        if (material != "16" && !LdrawColourOf(material)) {
            unknown += material + " ";
        }
    }
    return unknown;
}

TEST_CASE("every print loads from the shipped parts in known colours") {
    LdrawLibrary library(RR_LDRAW_DIR);

    for (const LdrawPrint& print : LdrawPrints()) {
        CAPTURE(print.torso);
        CHECK(UnknownColours(library, print).empty());
    }
}

TEST_CASE("every print names a game torso") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const LdrawPrint& print : LdrawPrints()) {
        CHECK_MESSAGE(vfs->Contains(std::format("MENUDATA/PARTDB/{}.BMP", print.torso)),
                      std::string(print.torso));
    }
}

}
}
