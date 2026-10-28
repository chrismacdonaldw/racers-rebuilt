#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/byte_reader.hpp"
#include "core/result.hpp"

namespace rr::sim {

inline constexpr std::uint64_t kFnvOffset = 0xCBF29CE484222325ULL;
inline constexpr std::uint64_t kFnvPrime = 0x100000001B3ULL;

[[nodiscard]] inline std::uint64_t HashBytes(std::span<const std::byte> bytes,
                                             std::uint64_t hash = kFnvOffset) {
    for (const std::byte byte : bytes) {
        hash = (hash ^ std::to_integer<std::uint64_t>(byte)) * kFnvPrime;
    }
    return hash;
}

inline void AppendU8(std::vector<std::byte>& out, std::uint8_t value) {
    out.push_back(std::byte{value});
}

inline void AppendU16(std::vector<std::byte>& out, std::uint16_t value) {
    AppendU8(out, static_cast<std::uint8_t>(value & 0xFFU));
    AppendU8(out, static_cast<std::uint8_t>(value >> 8U));
}

inline void AppendU32(std::vector<std::byte>& out, std::uint32_t value) {
    AppendU16(out, static_cast<std::uint16_t>(value & 0xFFFFU));
    AppendU16(out, static_cast<std::uint16_t>(value >> 16U));
}

inline void AppendU64(std::vector<std::byte>& out, std::uint64_t value) {
    AppendU32(out, static_cast<std::uint32_t>(value & 0xFFFFFFFFU));
    AppendU32(out, static_cast<std::uint32_t>(value >> 32U));
}

inline void AppendText(std::vector<std::byte>& out, std::string_view text) {
    AppendU16(out, static_cast<std::uint16_t>(text.size()));
    for (const char c : text) {
        out.push_back(static_cast<std::byte>(c));
    }
}

[[nodiscard]] inline Result<std::uint64_t> ReadU64(assets::ByteReader& reader) {
    Result<std::uint32_t> low = reader.ReadU32();
    Result<std::uint32_t> high = low.and_then([&](std::uint32_t) { return reader.ReadU32(); });
    return high.transform(
        [&](std::uint32_t value) { return (std::uint64_t{value} << 32U) | *low; });
}

[[nodiscard]] inline Result<std::string> ReadText(assets::ByteReader& reader) {
    return reader.ReadU16()
        .and_then([&](std::uint16_t size) { return reader.ReadBytes(size); })
        .transform([](std::span<const std::byte> bytes) {
            std::string text(bytes.size(), '\0');
            for (std::size_t i = 0; i < bytes.size(); ++i) {
                text[i] = static_cast<char>(bytes[i]);
            }
            return text;
        });
}

/// The reader starts after the version and stops before the trailing checksum.
[[nodiscard]] Result<assets::ByteReader> OpenChecked(std::span<const std::byte> bytes,
                                                     std::string_view magic, std::uint16_t version,
                                                     std::string_view what);

inline void AppendHeader(std::vector<std::byte>& out, std::string_view magic,
                         std::uint16_t version) {
    for (const char c : magic) {
        out.push_back(static_cast<std::byte>(c));
    }
    AppendU16(out, version);
}

inline void AppendChecksum(std::vector<std::byte>& out) {
    AppendU64(out, HashBytes(out));
}

}
