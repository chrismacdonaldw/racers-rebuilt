#include "assets/string_table.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <utility>

#include "assets/byte_reader.hpp"

namespace rr::assets {
namespace {

Result<std::vector<std::uint16_t>> ReadUnits(ByteReader& reader, std::size_t count) {
    std::vector<std::uint16_t> units;
    units.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        Result<std::uint16_t> unit = reader.ReadU16();
        if (!unit) {
            return std::unexpected(std::move(unit.error()));
        }
        units.push_back(*unit);
    }
    return units;
}

Result<std::u16string> StringAt(std::span<const std::uint16_t> data, std::uint16_t offset) {
    if (offset >= data.size()) {
        return Fail(
            ErrorCode::InvalidData,
            std::format("string offset {} is past the data of {} units", offset, data.size()));
    }
    const std::span<const std::uint16_t> rest = data.subspan(offset);
    const auto end = std::ranges::find(rest, std::uint16_t{0});
    if (end == rest.end()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("string at offset {} has no terminator", offset));
    }
    return std::u16string(rest.begin(), end);
}

}

Result<std::vector<std::u16string>> ParseStringTable(std::span<const std::byte> bytes) {
    ByteReader reader(bytes, 0);
    Result<std::uint16_t> count = reader.ReadU16();
    Result<std::uint16_t> length = count.and_then([&](std::uint16_t) { return reader.ReadU16(); });
    if (!length) {
        return std::unexpected(std::move(length.error()));
    }
    Result<std::vector<std::uint16_t>> offsets = ReadUnits(reader, *count);
    Result<std::vector<std::uint16_t>> data = offsets.and_then(
        [&](const std::vector<std::uint16_t>&) { return ReadUnits(reader, *length); });
    if (!data) {
        return std::unexpected(std::move(data.error()));
    }
    std::vector<std::u16string> strings;
    strings.reserve(offsets->size());
    for (const std::uint16_t offset : *offsets) {
        Result<std::u16string> text = StringAt(*data, offset);
        if (!text) {
            return std::unexpected(std::move(text.error()));
        }
        strings.push_back(std::move(*text));
    }
    return strings;
}

}
