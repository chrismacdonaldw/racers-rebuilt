#include "assets/ldraw_head.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "assets/face_drawing.hpp"

namespace rr::assets {
namespace {

constexpr Rgba8 kYellow{0xFA, 0xC8, 0x0A, 0xFF};
constexpr Rgba8 kRed{0xB4, 0x00, 0x00, 0xFF};
constexpr float kRadius = 0.5F;

struct Panel {
    float halfWidth = 0.05F;
    float top = 0.0F;
    float bottom = -1.0F;
    std::string colour = "4";
};

void AddPanel(GeometryAsset& head, const Panel& panel) {
    const auto first = static_cast<std::uint32_t>(head.vertices.size());
    const auto firstIndex = static_cast<std::uint32_t>(head.indices.size());
    for (const auto& [x, z] : {std::pair{-panel.halfWidth, panel.top},
                               {-panel.halfWidth, panel.bottom},
                               {panel.halfWidth, panel.bottom},
                               {panel.halfWidth, panel.top}}) {
        head.vertices.push_back({.position = {.x = x, .y = -kRadius, .z = z}});
    }
    head.indices.insert(head.indices.end(),
                        {first, first + 1, first + 2, first, first + 2, first + 3});
    head.ranges.push_back({.firstIndex = firstIndex,
                           .indexCount = 6,
                           .material = FindOrAddMaterial(head.materials, panel.colour)});
}

Rgba8 PixelAt(const ImageAsset& image, std::size_t x, std::size_t y) {
    const std::size_t at = ((y * image.widthPx) + x) * 4;
    return {std::to_integer<std::uint8_t>(image.rgba[at]),
            std::to_integer<std::uint8_t>(image.rgba[at + 1]),
            std::to_integer<std::uint8_t>(image.rgba[at + 2]),
            std::to_integer<std::uint8_t>(image.rgba[at + 3])};
}

TEST_CASE("a head print's front lands in the middle columns of the face") {
    GeometryAsset head;
    AddPanel(head, {});

    const ImageAsset face = DrawHeadPrint(head);

    REQUIRE(face.widthPx == kFaceDrawingSizePx);
    REQUIRE(face.heightPx == kFaceDrawingSizePx);
    CHECK(PixelAt(face, 128, 128) == kRed);
    CHECK(PixelAt(face, 128, 2) == kRed);
    CHECK(PixelAt(face, 128, 250) == kRed);
    CHECK(PixelAt(face, 2, 128) == kYellow);
    CHECK(PixelAt(face, 100, 128) == kYellow);
}

TEST_CASE("a head print draws its main colour first, in minifig yellow") {
    GeometryAsset head;
    AddPanel(head, {});
    AddPanel(head, {.halfWidth = 0.1F, .colour = "16"});

    const ImageAsset face = DrawHeadPrint(head);

    CHECK(PixelAt(face, 128, 128) == kRed);
}

TEST_CASE("a head print leaves out the top and the inside of the head") {
    GeometryAsset head;
    AddPanel(head, {});
    head.vertices.push_back({.position = {.x = -0.2F, .y = -0.2F, .z = 0.0F}});
    head.vertices.push_back({.position = {.x = 0.2F, .y = -0.2F, .z = 0.0F}});
    head.vertices.push_back({.position = {.x = 0.0F, .y = -0.45F, .z = -1.0F}});
    const auto first = static_cast<std::uint32_t>(head.vertices.size() - 3);
    head.indices.insert(head.indices.end(), {first, first + 1, first + 2});
    head.ranges.push_back(
        {.firstIndex = 6, .indexCount = 3, .material = FindOrAddMaterial(head.materials, "0")});

    const ImageAsset face = DrawHeadPrint(head);

    CHECK(PixelAt(face, 128, 128) == kRed);
}

TEST_CASE("a head with no side is plain yellow") {
    const ImageAsset face = DrawHeadPrint(GeometryAsset{});

    CHECK(PixelAt(face, 128, 128) == kYellow);
}

}
}
