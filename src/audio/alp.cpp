#include "audio/alp.hpp"

#include <algorithm>
#include <format>

#include "assets/byte_reader.hpp"

namespace rr::audio {
namespace {

using Tag = std::array<std::byte, 4>;

constexpr Tag kMagic{std::byte{'A'}, std::byte{'L'}, std::byte{'P'}, std::byte{' '}};
constexpr Tag kAdpcmTag{std::byte{'A'}, std::byte{'D'}, std::byte{'P'}, std::byte{'C'}};
constexpr std::size_t kAdpcmTagOffset = 8;
constexpr std::size_t kChannelCountOffset = 15;
constexpr std::uint64_t kPreambleBytes = 8;
constexpr std::uint64_t kShortHeaderBytes = 16;
constexpr std::uint64_t kRateHeaderBytes = 20;
constexpr std::uint32_t kStreamedRateHz = 22050;

constexpr std::int32_t kMinSample = -32768;
constexpr std::int32_t kMaxSample = 32767;
constexpr std::int32_t kMaxStepIndex = 88;
constexpr std::array<std::int32_t, kMaxStepIndex + 1> kStepTable{
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,
    25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,
    88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,   253,   279,
    307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,   876,   963,
    1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,  3327,
    3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};
constexpr std::array<std::int32_t, 8> kStepIndexAdjust{-1, -1, -1, -1, 2, 4, 6, 8};
constexpr std::uint8_t kMagnitudeBits = 0x7;
constexpr std::uint8_t kSignBit = 0x8;

bool HasTag(std::span<const std::byte> bytes, std::size_t offset, const Tag& tag) {
    return bytes.size() >= offset + tag.size() &&
           std::ranges::equal(bytes.subspan(offset, tag.size()), tag);
}

Result<std::uint32_t> ReadSampleRate(assets::ByteReader& reader, std::uint64_t dataOffset) {
    if (dataOffset == kShortHeaderBytes) {
        return kStreamedRateHz;
    }
    Result<std::uint32_t> rate = reader.ReadU32();
    if (!rate || *rate == 0) {
        return Fail(ErrorCode::InvalidData, "ALP header has no sample rate");
    }
    return rate;
}

}

Result<AlpSound> ParseAlp(std::span<const std::byte> bytes) {
    if (!HasTag(bytes, 0, kMagic) || !HasTag(bytes, kAdpcmTagOffset, kAdpcmTag)) {
        return Fail(ErrorCode::InvalidData, "not an ALP ADPCM file");
    }
    assets::ByteReader sizeReader(bytes, kMagic.size());
    assets::ByteReader formatReader(bytes, kChannelCountOffset);
    Result<std::uint32_t> headerBytes = sizeReader.ReadU32();
    Result<std::uint8_t> channelCount = formatReader.ReadU8();
    if (!headerBytes || !channelCount) {
        return Fail(ErrorCode::InvalidData, "ALP header is cut short");
    }
    const std::uint64_t dataOffset = *headerBytes + kPreambleBytes;
    const bool hasPartialRate = dataOffset > kShortHeaderBytes && dataOffset < kRateHeaderBytes;
    if (dataOffset < kShortHeaderBytes || hasPartialRate || dataOffset > bytes.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("ALP header size {} is out of range", *headerBytes));
    }
    if (*channelCount != 1 && *channelCount != 2) {
        return Fail(ErrorCode::InvalidData, std::format("ALP file has {} channels", *channelCount));
    }
    Result<std::uint32_t> sampleRateHz = ReadSampleRate(formatReader, dataOffset);
    if (!sampleRateHz) {
        return std::unexpected(std::move(sampleRateHz.error()));
    }

    std::span<const std::byte> data = bytes.subspan(dataOffset);
    data = data.first(data.size() - (data.size() % *channelCount));
    return AlpSound{.format = {.channelCount = *channelCount, .sampleRateHz = *sampleRateHz},
                    .data = data};
}

AlpDecoder::AlpDecoder(AlpSound sound) : sound_(sound) {}

std::uint64_t AlpDecoder::FrameCount() const {
    return sound_.data.size() * 2 / sound_.format.channelCount;
}

std::uint64_t AlpDecoder::Read(std::span<std::int16_t> interleaved) noexcept {
    const std::size_t channelCount = sound_.format.channelCount;
    const std::uint64_t frames =
        std::min<std::uint64_t>(interleaved.size() / channelCount, FrameCount() - cursor_);
    std::size_t out = 0;
    for (std::uint64_t frame = 0; frame < frames; ++frame, ++cursor_) {
        const std::size_t byteIndex = static_cast<std::size_t>(cursor_ / 2) * channelCount;
        const bool isHighNibble = cursor_ % 2 == 0;
        for (std::size_t channel = 0; channel < channelCount; ++channel) {
            const auto byte = std::to_integer<std::uint8_t>(sound_.data[byteIndex + channel]);
            const auto nibble = static_cast<std::uint8_t>(isHighNibble ? byte >> 4 : byte & 0xF);
            interleaved[out++] = channels_[channel].Expand(nibble);
        }
    }
    return frames;
}

void AlpDecoder::Seek(std::uint64_t frame) noexcept {
    constexpr std::size_t kScratchSamples = 512;
    frame = std::min(frame, FrameCount());
    if (frame < cursor_) {
        Rewind();
    }
    std::array<std::int16_t, kScratchSamples> scratch{};
    const std::size_t channelCount = sound_.format.channelCount;
    while (cursor_ < frame) {
        const std::uint64_t frames =
            std::min<std::uint64_t>(frame - cursor_, kScratchSamples / channelCount);
        Read(std::span(scratch).first(static_cast<std::size_t>(frames) * channelCount));
    }
}

void AlpDecoder::Rewind() noexcept {
    channels_ = {};
    cursor_ = 0;
}

std::int16_t AlpDecoder::Channel::Expand(std::uint8_t nibble) noexcept {
    const std::uint8_t magnitude = nibble & kMagnitudeBits;
    const std::int32_t delta = (magnitude * kStepTable[static_cast<std::size_t>(stepIndex)]) >> 2;
    sample = (nibble & kSignBit) != 0 ? std::max(sample - delta, kMinSample)
                                      : std::min(sample + delta, kMaxSample);
    stepIndex = std::clamp(stepIndex + kStepIndexAdjust[magnitude], 0, kMaxStepIndex);
    return static_cast<std::int16_t>(sample);
}

}
