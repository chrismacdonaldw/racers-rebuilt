#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace rr::test {

inline void AppendU32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xFF));
    }
}

inline void AppendText(std::vector<std::byte>& bytes, std::string_view text) {
    for (const char c : text) {
        bytes.push_back(static_cast<std::byte>(c));
    }
}

/// Without a rate it gets the 16-byte header that `.tun` music uses.
inline std::vector<std::byte> BuildAlp(std::uint8_t channelCount,
                                       std::optional<std::uint32_t> sampleRateHz,
                                       std::span<const std::uint8_t> payload) {
    std::vector<std::byte> bytes;
    AppendText(bytes, "ALP ");
    AppendU32(bytes, sampleRateHz ? 12 : 8);
    AppendText(bytes, std::string_view("ADPCM\0\0", 7));
    bytes.push_back(static_cast<std::byte>(channelCount));
    if (sampleRateHz) {
        AppendU32(bytes, *sampleRateHz);
    }
    for (const std::uint8_t value : payload) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

}
