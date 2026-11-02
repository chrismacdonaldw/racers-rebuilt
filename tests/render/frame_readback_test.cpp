#include "render/frame_readback.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <vector>

namespace rr::render {
namespace {

constexpr std::array kPixels{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{0},
                             std::byte{4}, std::byte{5}, std::byte{6}, std::byte{7}};

}

TEST_CASE("a BGRA frame reads back as opaque RGBA") {
    const Result<std::vector<std::byte>> rgba =
        ToRgba(kPixels, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM);

    REQUIRE(rgba.has_value());
    CHECK(*rgba == std::vector{std::byte{3}, std::byte{2}, std::byte{1}, std::byte{0xFF},
                               std::byte{6}, std::byte{5}, std::byte{4}, std::byte{0xFF}});
}

TEST_CASE("an RGBA frame keeps its channel order") {
    const Result<std::vector<std::byte>> rgba =
        ToRgba(kPixels, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB);

    REQUIRE(rgba.has_value());
    CHECK(*rgba == std::vector{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{0xFF},
                               std::byte{4}, std::byte{5}, std::byte{6}, std::byte{0xFF}});
}

TEST_CASE("a frame in another format can't be read back") {
    CHECK_FALSE(ToRgba(kPixels, SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM).has_value());
}

}
