#include "app/racer/shadow_silhouette.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rr::app {
namespace {

constexpr std::size_t kChannels = 4;

std::byte AlphaAt(const assets::ImageAsset& image, std::uint32_t column, std::uint32_t row) {
    return image.rgba.at((((row * image.widthPx) + column) * kChannels) + 3);
}

std::size_t Covered(const assets::ImageAsset& image) {
    std::size_t covered = 0;
    for (std::size_t i = 3; i < image.rgba.size(); i += kChannels) {
        covered += image.rgba[i] == std::byte{255} ? 1 : 0;
    }
    return covered;
}

std::vector<Vector3> Square(float lowX, float highX, float lowY, float highY) {
    return {{.x = lowX, .y = lowY}, {.x = highX, .y = lowY},  {.x = highX, .y = highY},
            {.x = lowX, .y = lowY}, {.x = highX, .y = highY}, {.x = lowX, .y = highY}};
}

TEST_CASE("an empty silhouette is clear") {
    const assets::ImageAsset image = ShadowSilhouette({}, 8.0F, 8.0F);

    CHECK(image.widthPx == kSilhouettePx);
    CHECK(image.heightPx == kSilhouettePx);
    CHECK(Covered(image) == 0);
    CHECK(image.rgba.size() == std::size_t{kSilhouettePx} * kSilhouettePx * kChannels);
}

TEST_CASE("a centred square fills the middle block in the original's dark grey") {
    const assets::ImageAsset image = ShadowSilhouette(Square(-2.0F, 2.0F, -2.0F, 2.0F), 8.0F, 8.0F);

    CHECK(Covered(image) == 32 * 32);
    CHECK(AlphaAt(image, 16, 16) == std::byte{255});
    CHECK(AlphaAt(image, 47, 47) == std::byte{255});
    CHECK(AlphaAt(image, 15, 16) == std::byte{0});
    CHECK(image.rgba.at(((16 * kSilhouettePx) + 16) * kChannels) == std::byte{8});
}

TEST_CASE("rows run forward and columns toward the car's left") {
    const assets::ImageAsset front = ShadowSilhouette(Square(3.0F, 4.0F, -1.0F, 1.0F), 8.0F, 8.0F);
    const assets::ImageAsset left = ShadowSilhouette(Square(-1.0F, 1.0F, 3.0F, 4.0F), 8.0F, 8.0F);

    CHECK(AlphaAt(front, 32, 60) == std::byte{255});
    CHECK(AlphaAt(front, 32, 3) == std::byte{0});
    CHECK(AlphaAt(left, 60, 32) == std::byte{255});
    CHECK(AlphaAt(left, 3, 32) == std::byte{0});
}

TEST_CASE("a triangle with a corner outside the window is dropped whole") {
    const std::vector<Vector3> corners{
        {.x = -1.0F, .y = -1.0F}, {.x = 1.0F, .y = -1.0F}, {.x = 0.0F, .y = 5.0F}};

    CHECK(Covered(ShadowSilhouette(corners, 8.0F, 8.0F)) == 0);
}

TEST_CASE("the capture window stretches to the length and width given") {
    const assets::ImageAsset image =
        ShadowSilhouette(Square(-2.0F, 2.0F, -2.0F, 2.0F), 16.0F, 8.0F);

    CHECK(Covered(image) == 16 * 32);
}

}
}
