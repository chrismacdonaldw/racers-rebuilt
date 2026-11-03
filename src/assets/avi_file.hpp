#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/duration.hpp"
#include "core/result.hpp"

namespace rr::assets {

struct AviFrameRate {
    std::uint32_t frames = 0;
    std::uint32_t seconds = 1;
};

struct AviVideo {
    std::uint32_t codec = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    AviFrameRate rate;
    std::vector<std::span<const std::byte>> frames;
};

struct AviAudio {
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::vector<std::int16_t> samples;

    friend bool operator==(const AviAudio&, const AviAudio&) = default;
};

struct AviFile {
    AviVideo video;
    std::optional<AviAudio> audio;
};

/// `code` is four characters.
[[nodiscard]] constexpr std::uint32_t FourCc(std::string_view code) {
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(code.at(i))) << (8U * i);
    }
    return value;
}

[[nodiscard]] std::size_t FrameAt(const AviVideo& video, Microseconds elapsed);

/// Reads the first video and 16-bit PCM streams. Frames borrow `bytes`; an empty one repeats.
[[nodiscard]] Result<AviFile> ParseAvi(std::span<const std::byte> bytes);

}
