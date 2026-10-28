#include "audio/alp.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <vector>

#include "audio/alp_builder.hpp"

namespace rr::audio {
namespace {

constexpr std::array<std::uint8_t, 2> kRampThenFall{0x77, 0xF0};

AlpDecoder DecoderFor(const std::vector<std::byte>& file) {
    const Result<AlpSound> sound = ParseAlp(file);
    REQUIRE(sound.has_value());
    return AlpDecoder(sound.value_or(AlpSound{}));
}

AlpDecoder DecoderFor(std::vector<std::byte>&& file) = delete;

std::vector<std::int16_t> ReadAll(AlpDecoder& decoder) {
    std::vector<std::int16_t> samples(
        static_cast<std::size_t>(decoder.FrameCount() - decoder.Cursor()) *
        decoder.Format().channelCount);
    CHECK(decoder.Read(samples) * decoder.Format().channelCount == samples.size());
    return samples;
}

TEST_CASE("a 20-byte ALP header gives the channel count and sample rate") {
    const std::vector<std::byte> file = test::BuildAlp(1, 11025, kRampThenFall);

    const Result<AlpSound> sound = ParseAlp(file);

    REQUIRE(sound.has_value());
    CHECK(sound->format.channelCount == 1);
    CHECK(sound->format.sampleRateHz == 11025);
    CHECK(sound->data.size() == kRampThenFall.size());
}

TEST_CASE("a 16-byte ALP header has no rate, so it plays at the 22050 Hz music rate") {
    const std::vector<std::byte> file = test::BuildAlp(2, std::nullopt, kRampThenFall);

    const Result<AlpSound> sound = ParseAlp(file);

    REQUIRE(sound.has_value());
    CHECK(sound->format.channelCount == 2);
    CHECK(sound->format.sampleRateHz == 22050);
}

TEST_CASE("files that aren't usable ALP ADPCM are rejected") {
    std::vector<std::byte> file = test::BuildAlp(3, 11025, kRampThenFall);
    CHECK(ParseAlp(file).error().message.find("3 channels") != std::string::npos);

    file = test::BuildAlp(1, 11025, kRampThenFall);
    file[4] = std::byte{0x40};
    CHECK(ParseAlp(file).error().message.find("out of range") != std::string::npos);

    file[0] = std::byte{'X'};
    CHECK(ParseAlp(file).error().code == ErrorCode::InvalidData);
    CHECK(ParseAlp({}).error().code == ErrorCode::InvalidData);
}

TEST_CASE("mono ADPCM decodes the high nibble first, with the step scaled by a quarter") {
    const std::vector<std::byte> file = test::BuildAlp(1, 11025, kRampThenFall);
    AlpDecoder decoder = DecoderFor(file);

    CHECK(ReadAll(decoder) == std::vector<std::int16_t>{12, 40, -19, -19});
}

TEST_CASE("stereo ADPCM gives each channel alternate bytes") {
    const std::vector<std::byte> file = test::BuildAlp(2, std::nullopt, kRampThenFall);
    AlpDecoder decoder = DecoderFor(file);

    CHECK(decoder.FrameCount() == 2);
    CHECK(ReadAll(decoder) == std::vector<std::int16_t>{12, -12, 40, -12});
}

TEST_CASE("decoded samples clamp to the 16-bit range") {
    const std::vector<std::uint8_t> rising(64, 0x77);
    const std::vector<std::uint8_t> falling(64, 0xFF);
    const std::vector<std::byte> risingFile = test::BuildAlp(1, 11025, rising);
    const std::vector<std::byte> fallingFile = test::BuildAlp(1, 11025, falling);
    AlpDecoder up = DecoderFor(risingFile);
    AlpDecoder down = DecoderFor(fallingFile);

    const std::vector<std::int16_t> upSamples = ReadAll(up);
    const std::vector<std::int16_t> downSamples = ReadAll(down);

    CHECK(std::ranges::is_sorted(upSamples));
    CHECK(upSamples.back() == 32767);
    CHECK(downSamples.back() == -32768);
}

TEST_CASE("seeking gives the same samples as reading straight through") {
    const std::vector<std::uint8_t> payload{0x71, 0x9A, 0x3C, 0xF2, 0x05, 0xE7, 0x48, 0xB3};
    const std::vector<std::byte> file = test::BuildAlp(1, 11025, payload);
    AlpDecoder decoder = DecoderFor(file);
    const std::vector<std::int16_t> straight = ReadAll(decoder);

    decoder.Seek(5);
    CHECK(ReadAll(decoder) == std::vector<std::int16_t>(straight.begin() + 5, straight.end()));

    decoder.Seek(1);
    CHECK(ReadAll(decoder) == std::vector<std::int16_t>(straight.begin() + 1, straight.end()));
}

TEST_CASE("reading past the end returns only the frames left") {
    const std::vector<std::byte> file =
        test::BuildAlp(2, std::nullopt, std::array<std::uint8_t, 3>{});
    AlpDecoder decoder = DecoderFor(file);
    std::array<std::int16_t, 16> samples{};

    CHECK(decoder.Read(samples) == 2);
    CHECK(decoder.Read(samples) == 0);
}

}
}
