#include "assets/byte_reader.hpp"

#include <format>
#include <utility>

namespace rr::assets {

ByteReader::ByteReader(std::span<const std::byte> bytes, std::size_t position)
    : bytes_(bytes), position_(position) {}

std::size_t ByteReader::Remaining() const {
    return position_ < bytes_.size() ? bytes_.size() - position_ : 0;
}

Result<std::uint32_t> ByteReader::ReadU32() {
    Result<std::span<const std::byte>> bytes = ReadBytes(sizeof(std::uint32_t));
    if (!bytes) {
        return std::unexpected(std::move(bytes.error()));
    }
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < bytes->size(); ++i) {
        value |= std::to_integer<std::uint32_t>((*bytes)[i]) << (8U * i);
    }
    return value;
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

}
