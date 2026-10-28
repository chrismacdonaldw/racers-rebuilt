#include "app/music_folder_source.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>
#include <vector>

#include "assets/jam_builder.hpp"
#include "audio/alp_builder.hpp"
#include "audio/vorbis_tone.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

constexpr std::uint32_t kRateHz = 22050;
constexpr std::uint32_t kWavFrames = 8;

void AppendU16(std::vector<std::byte>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
    bytes.push_back(static_cast<std::byte>(value >> 8U));
}

std::vector<std::byte> BuildMonoWav() {
    constexpr std::uint32_t kFmtBytes = 16;
    constexpr std::uint32_t kDataBytes = kWavFrames * 2;
    std::vector<std::byte> bytes;
    test::AppendText(bytes, "RIFF");
    test::AppendU32(bytes, 4 + (8 + kFmtBytes) + (8 + kDataBytes));
    test::AppendText(bytes, "WAVEfmt ");
    test::AppendU32(bytes, kFmtBytes);
    AppendU16(bytes, 1);
    AppendU16(bytes, 1);
    test::AppendU32(bytes, kRateHz);
    test::AppendU32(bytes, kRateHz * 2);
    AppendU16(bytes, 2);
    AppendU16(bytes, 16);
    test::AppendText(bytes, "data");
    test::AppendU32(bytes, kDataBytes);
    bytes.resize(bytes.size() + kDataBytes);
    return bytes;
}

class FolderFixture {
public:
    FolderFixture() : source_(folder_.Path()) {}

    void Write(std::string_view name, std::string_view contents) {
        std::ofstream(folder_.Path() / name, std::ios::binary) << contents;
    }
    void WriteWav(std::string_view name) { Write(name, test::AsText(BuildMonoWav())); }

protected:
    test::TempFolder folder_;
    MusicFolderSource source_;
};

TEST_CASE_FIXTURE(FolderFixture, "a replacement OGG decodes in full") {
    Write("circuit1.ogg", test::AsText(test::VorbisTone()));

    const Result<audio::MusicTrack> track = source_.Open("circuit1");

    REQUIRE(track.has_value());
    CHECK(track->stream->Format().sampleRateHz == test::kVorbisToneRateHz);
    CHECK(track->stream->FrameCount() == test::kVorbisToneFrames);
    std::vector<std::int16_t> samples(test::kVorbisToneFrames);
    CHECK(track->stream->Read(samples) == test::kVorbisToneFrames);
}

TEST_CASE_FIXTURE(FolderFixture, "a replacement WAV loops whole without a sidecar") {
    WriteWav("circuit1.wav");

    const Result<audio::MusicTrack> track = source_.Open("circuit1");

    REQUIRE(track.has_value());
    CHECK(track->stream->FrameCount() == kWavFrames);
    CHECK(track->loop.startFrame == 0);
    CHECK_FALSE(track->loop.endFrame.has_value());
}

TEST_CASE_FIXTURE(FolderFixture, "a missing replacement is not found") {
    WriteWav("circuit2.wav");

    CHECK(source_.Open("circuit1").error().code == ErrorCode::NotFound);
}

TEST_CASE_FIXTURE(FolderFixture, "a replacement that can't be decoded is broken") {
    Write("circuit1.ogg", "not vorbis");

    CHECK(source_.Open("circuit1").error().code == ErrorCode::InvalidData);
}

TEST_CASE_FIXTURE(FolderFixture, "a sidecar sets the replacement's loop points") {
    WriteWav("circuit1.wav");
    Write("circuit1.toml", "loop_start = 2\nloop_end = 6\n");

    const Result<audio::MusicTrack> track = source_.Open("circuit1");

    REQUIRE(track.has_value());
    CHECK(track->loop.startFrame == 2);
    CHECK(track->loop.endFrame == 6);
}

TEST_CASE_FIXTURE(FolderFixture, "a loop start past the end loops the whole replacement") {
    WriteWav("circuit1.wav");
    Write("circuit1.toml", "loop_start = 100\n");

    const Result<audio::MusicTrack> track = source_.Open("circuit1");

    REQUIRE(track.has_value());
    CHECK(track->loop.startFrame == 0);
}

}
}
