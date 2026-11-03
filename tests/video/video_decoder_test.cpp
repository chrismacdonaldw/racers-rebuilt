#include "video/video_decoder.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "game_files.hpp"
#include "platform/mapped_file.hpp"
#include "video/yuv.hpp"

namespace rr::video {
namespace {

std::array<std::uint8_t, 4> Convert(std::uint8_t y, std::uint8_t u, std::uint8_t v) {
    const std::array<std::uint8_t, 1> luma{y};
    const std::array<std::uint8_t, 1> blue{u};
    const std::array<std::uint8_t, 1> red{v};
    std::array<std::uint8_t, 4> rgba{};
    YuvToRgba({.y = {.samples = luma, .stride = 1},
               .u = {.samples = blue, .stride = 1},
               .v = {.samples = red, .stride = 1},
               .width = 1,
               .height = 1},
              rgba);
    return rgba;
}

Result<std::vector<std::uint8_t>> DecodeOpening(const std::filesystem::path& path) {
    constexpr std::size_t kFrames = 60;
    const Result<platform::MappedFile> file = platform::MappedFile::Open(path);
    const Result<assets::AviFile> avi =
        file ? assets::ParseAvi(file->Bytes()) : std::unexpected(file.error());
    Result<VideoDecoder> decoder =
        avi ? VideoDecoder::Create(avi->video) : std::unexpected(avi.error());
    if (!decoder) {
        return std::unexpected(decoder.error());
    }
    const std::span<const std::span<const std::byte>> frames = avi->video.frames;
    for (const std::span<const std::byte> frame : frames.first(std::min(kFrames, frames.size()))) {
        if (Result<void> decoded = decoder->Decode(frame); !decoded) {
            return std::unexpected(decoded.error());
        }
    }
    const std::span<const std::uint8_t> picture = decoder->Picture();
    return std::vector<std::uint8_t>(picture.begin(), picture.end());
}

}

TEST_CASE("limited range YUV converts to RGBA") {
    CHECK(Convert(16, 128, 128) == std::array<std::uint8_t, 4>{0, 0, 0, 255});
    CHECK(Convert(235, 128, 128) == std::array<std::uint8_t, 4>{255, 255, 255, 255});
    CHECK(Convert(81, 90, 240) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
}

TEST_CASE("each chroma sample covers a square of luma samples") {
    const std::array<std::uint8_t, 4> luma{16, 16, 16, 16};
    const std::array<std::uint8_t, 1> blue{128};
    const std::array<std::uint8_t, 1> red{240};
    std::array<std::uint8_t, 16> rgba{};
    YuvToRgba({.y = {.samples = luma, .stride = 2},
               .u = {.samples = blue, .stride = 1},
               .v = {.samples = red, .stride = 1},
               .width = 2,
               .height = 2,
               .chromaShift = 1},
              rgba);
    for (std::size_t pixel = 0; pixel < 4; ++pixel) {
        CHECK(rgba.at(pixel * 4) > 100);
    }
}

TEST_CASE("only Indeo 5 video decodes") {
    const Result<VideoDecoder> decoder = VideoDecoder::Create(
        {.codec = assets::FourCc("cvid"), .width = 16, .height = 16, .rate = {.frames = 15}});
    REQUIRE_FALSE(decoder.has_value());
    CHECK(decoder.error().code == ErrorCode::NotFound);
}

TEST_CASE("the retail intro's first frames decode to a picture") {
    const std::optional<std::filesystem::path> gameFolder = test::GameFolder();
    if (!gameFolder) {
        MESSAGE("RR_GAME_PATH isn't set, skipping");
        return;
    }
    const Result<std::vector<std::uint8_t>> picture = DecodeOpening(*gameFolder / "introcmp.avi");
    REQUIRE(picture.has_value());
    CHECK(picture->size() == std::size_t{640} * 480 * 4);
    CHECK(std::ranges::any_of(*picture, [](std::uint8_t channel) { return channel > 64; }));
}

}
