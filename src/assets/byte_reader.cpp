#include "assets/byte_reader.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace rr::assets {

ByteReader::ByteReader(std::span<const std::byte> bytes, std::size_t position)
    : bytes_(bytes), position_(position) {}

std::size_t ByteReader::Remaining() const {
    return position_ < bytes_.size() ? bytes_.size() - position_ : 0;
}

template <typename T>
Result<T> ByteReader::ReadUnsigned() {
    Result<std::span<const std::byte>> bytes = ReadBytes(sizeof(T));
    if (!bytes) {
        return std::unexpected(std::move(bytes.error()));
    }
    T value = 0;
    for (std::size_t i = 0; i < bytes->size(); ++i) {
        value |= static_cast<T>(std::to_integer<T>((*bytes)[i]) << (8U * i));
    }
    return value;
}

Result<std::uint8_t> ByteReader::ReadU8() {
    return ReadUnsigned<std::uint8_t>();
}

Result<std::uint16_t> ByteReader::ReadU16() {
    return ReadUnsigned<std::uint16_t>();
}

Result<std::uint32_t> ByteReader::ReadU32() {
    return ReadUnsigned<std::uint32_t>();
}

Result<std::span<const std::byte>> ByteReader::ReadBytes(std::size_t count) {
    if (count > Remaining()) {
        return Fail(ErrorCode::InvalidData, std::format("can't read {} bytes at offset {} of {}",
                                                        count, position_, bytes_.size()));
    }
    const std::span<const std::byte> bytes = bytes_.subspan(position_, count);
    position_ += count;
    return bytes;
}

Result<std::span<const std::byte>> ByteReader::ReadUntil(std::byte terminator) {
    const std::span<const std::byte> rest = bytes_.subspan(std::min(position_, bytes_.size()));
    const auto end = std::ranges::find(rest, terminator);
    if (end == rest.end()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("no terminator after offset {} of {}", position_, bytes_.size()));
    }
    const std::span<const std::byte> bytes =
        rest.first(static_cast<std::size_t>(end - rest.begin()));
    position_ += bytes.size() + 1;
    return bytes;
}

}
