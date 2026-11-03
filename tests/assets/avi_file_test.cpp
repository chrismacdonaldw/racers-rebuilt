#include "assets/avi_file.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "game_files.hpp"
#include "platform/mapped_file.hpp"

namespace rr::assets {
namespace {

using Bytes = std::vector<std::byte>;

void PutU16(Bytes& out, std::uint16_t value) {
    out.push_back(static_cast<std::byte>(value & 0xFFU));
    out.push_back(static_cast<std::byte>(value >> 8U));
}

void PutU32(Bytes& out, std::uint32_t value) {
    PutU16(out, static_cast<std::uint16_t>(value & 0xFFFFU));
    PutU16(out, static_cast<std::uint16_t>(value >> 16U));
}

Bytes Chunk(std::string_view id, const Bytes& data) {
    Bytes out;
    PutU32(out, FourCc(id));
    PutU32(out, static_cast<std::uint32_t>(data.size()));
    out.insert(out.end(), data.begin(), data.end());
    if (data.size() % 2 != 0) {
        out.push_back(std::byte{0});
    }
    return out;
}

Bytes List(std::string_view id, std::string_view type, const std::vector<Bytes>& children) {
    Bytes data;
    PutU32(data, FourCc(type));
    for (const Bytes& child : children) {
        data.insert(data.end(), child.begin(), child.end());
    }
    return Chunk(id, data);
}

Bytes StreamHeader(std::string_view type, std::uint32_t scale, std::uint32_t rate) {
    Bytes data;
    PutU32(data, FourCc(type));
    data.resize(20);
    PutU32(data, scale);
    PutU32(data, rate);
    data.resize(56);
    return Chunk("strh", data);
}

struct VideoShape {
    std::uint32_t width = 640;
    std::uint32_t height = 480;
    std::uint32_t framesPerSecond = 15;
};

Bytes VideoFormat(const VideoShape& shape) {
    Bytes data;
    PutU32(data, 40);
    PutU32(data, shape.width);
    PutU32(data, shape.height);
    PutU16(data, 1);
    PutU16(data, 24);
    PutU32(data, FourCc("IV50"));
    data.resize(40);
    return Chunk("strf", data);
}

Bytes AudioFormat() {
    Bytes data;
    PutU16(data, 1);
    PutU16(data, 2);
    PutU32(data, 22050);
    PutU32(data, 88200);
    PutU16(data, 4);
    PutU16(data, 16);
    return Chunk("strf", data);
}

Bytes TinyAvi(VideoShape shape = {}) {
    const Bytes header = List(
        "LIST", "hdrl",
        {Chunk("avih", Bytes(56)),
         List("LIST", "strl", {StreamHeader("vids", 1, shape.framesPerSecond), VideoFormat(shape)}),
         List("LIST", "strl", {StreamHeader("auds", 1, 22050), AudioFormat()})});
    Bytes samples;
    PutU16(samples, 0x1234);
    PutU16(samples, 0xFFFF);
    const Bytes movie = List("LIST", "movi",
                             {Chunk("00dc", Bytes(3, std::byte{7})), Chunk("01wb", samples),
                              List("LIST", "rec ", {Chunk("00dc", {}), Chunk("JUNK", Bytes(5))})});
    return List("RIFF", "AVI ", {header, movie, Chunk("idx1", Bytes(16))});
}

struct AviSummary {
    std::size_t frames = 0;
    std::uint32_t framesPerSecond = 0;
    std::size_t samples = 0;

    friend bool operator==(const AviSummary&, const AviSummary&) = default;
};

std::optional<AviSummary> Summarize(const std::filesystem::path& path) {
    const Result<platform::MappedFile> file = platform::MappedFile::Open(path);
    const Result<AviFile> avi = file ? ParseAvi(file->Bytes()) : std::unexpected(file.error());
    if (!avi || !avi->audio) {
        return std::nullopt;
    }
    return AviSummary{.frames = avi->video.frames.size(),
                      .framesPerSecond = avi->video.rate.frames / avi->video.rate.seconds,
                      .samples = avi->audio->samples.size()};
}

}

TEST_CASE("an AVI file's video frames and PCM samples are read in order") {
    const Bytes bytes = TinyAvi();
    const Result<AviFile> avi = ParseAvi(bytes);
    REQUIRE(avi.has_value());
    CHECK(avi->video.codec == FourCc("IV50"));
    CHECK(avi->video.width == 640);
    CHECK(avi->video.height == 480);
    REQUIRE(avi->video.frames.size() == 2);
    CHECK(avi->video.frames[0].size() == 3);
    CHECK(avi->video.frames[1].empty());
    CHECK(avi->audio ==
          std::optional(AviAudio{.channels = 2, .sampleRate = 22050, .samples = {0x1234, -1}}));
}

TEST_CASE("an AVI frame is picked by its time") {
    const AviVideo video{.rate = {.frames = 15, .seconds = 1}};
    CHECK(FrameAt(video, Microseconds{0}) == 0);
    CHECK(FrameAt(video, Microseconds{66'666}) == 0);
    CHECK(FrameAt(video, Microseconds{66'667}) == 1);
    CHECK(FrameAt(video, Microseconds{72'000'000}) == 1080);
}

TEST_CASE("a file that isn't an AVI or runs short is rejected") {
    Bytes bytes = TinyAvi();
    CHECK_FALSE(ParseAvi(Bytes(bytes.begin(), bytes.begin() + 40)).has_value());
    bytes[8] = std::byte{'W'};
    CHECK_FALSE(ParseAvi(bytes).has_value());
}

TEST_CASE("an AVI video with no size, a huge size or a huge frame rate is rejected") {
    CHECK_FALSE(ParseAvi(TinyAvi({.width = 0})).has_value());
    CHECK_FALSE(ParseAvi(TinyAvi({.height = 0})).has_value());
    CHECK_FALSE(ParseAvi(TinyAvi({.width = 65535, .height = 65535})).has_value());
    CHECK_FALSE(ParseAvi(TinyAvi({.framesPerSecond = 1001})).has_value());
    CHECK(ParseAvi(TinyAvi({.width = 4096, .height = 4096, .framesPerSecond = 1000})).has_value());
}

TEST_CASE("the retail intro videos are Indeo 5 with 16-bit stereo sound") {
    const std::optional<std::filesystem::path> gameFolder = test::GameFolder();
    if (!gameFolder) {
        MESSAGE("RR_GAME_PATH isn't set, skipping");
        return;
    }
    CHECK(Summarize(*gameFolder / "lmicmp.avi") ==
          AviSummary{.frames = 237, .framesPerSecond = 15, .samples = 696'780});
    CHECK(Summarize(*gameFolder / "HVSCmp.avi") ==
          AviSummary{.frames = 337, .framesPerSecond = 15, .samples = 990'780});
    CHECK(Summarize(*gameFolder / "introcmp.avi") ==
          AviSummary{.frames = 1080, .framesPerSecond = 15, .samples = 3'175'200});
}

}
