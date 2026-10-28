#include "sim/replay/replay_bytes.hpp"

#include <format>

namespace rr::sim {

Result<assets::ByteReader> OpenChecked(std::span<const std::byte> bytes, std::string_view magic,
                                       std::uint16_t version, std::string_view what) {
    constexpr std::size_t kChecksumBytes = 8;
    const std::size_t headerBytes = magic.size() + 2;
    if (bytes.size() < headerBytes + kChecksumBytes) {
        return Fail(ErrorCode::InvalidData, std::format("{} is truncated", what));
    }
    for (std::size_t i = 0; i < magic.size(); ++i) {
        if (bytes[i] != static_cast<std::byte>(magic[i])) {
            return Fail(ErrorCode::InvalidData, std::format("{} has the wrong magic", what));
        }
    }
    const std::span<const std::byte> body = bytes.first(bytes.size() - kChecksumBytes);
    assets::ByteReader trailer(bytes, body.size());
    if (ReadU64(trailer) != HashBytes(body)) {
        return Fail(ErrorCode::InvalidData, std::format("{} fails its checksum", what));
    }
    assets::ByteReader reader(body, magic.size());
    const Result<std::uint16_t> found = reader.ReadU16();
    if (found != version) {
        return Fail(ErrorCode::InvalidData, std::format("{} is format version {}, expected {}",
                                                        what, found.value_or(0), version));
    }
    return reader;
}

}
