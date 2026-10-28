#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "core/result.hpp"

namespace rr::assets {

/// Little-endian cursor. A read past the end fails and leaves the position unchanged.
class ByteReader {
public:
    ByteReader(std::span<const std::byte> bytes, std::size_t position);

    [[nodiscard]] std::size_t Position() const { return position_; }
    [[nodiscard]] std::size_t Remaining() const;

    [[nodiscard]] Result<std::uint8_t> ReadU8();
    [[nodiscard]] Result<std::uint16_t> ReadU16();
    [[nodiscard]] Result<std::uint32_t> ReadU32();
    [[nodiscard]] Result<std::span<const std::byte>> ReadBytes(std::size_t count);

    /// Consumes the terminator but doesn't return it.
    [[nodiscard]] Result<std::span<const std::byte>> ReadUntil(std::byte terminator);

private:
    template <typename T>
    Result<T> ReadUnsigned();

    std::span<const std::byte> bytes_;
    std::size_t position_;
};

}
