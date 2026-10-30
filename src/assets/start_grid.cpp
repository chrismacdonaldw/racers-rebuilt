#include "assets/start_grid.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kListField = 0x27;
constexpr std::size_t kMaxSlots = 255;

struct StoredSlot {
    std::optional<Vector3> position;
    std::optional<std::array<Vector3, 2>> axes;
};

constexpr std::array kSlotFields{
    Field<&StoredSlot::position>(0x28),
    Field<&StoredSlot::axes>(0x29),
};

struct SlotTable {
    std::vector<StartSlot> slots;
    std::vector<bool> isRead;
};

Result<void> Place(const StoredSlot& stored, std::int32_t index, SlotTable& table) {
    if (index < 0 || std::cmp_greater_equal(index, table.slots.size())) {
        return Fail(ErrorCode::InvalidData,
                    std::format("start slot {} of {}", index, table.slots.size()));
    }
    const auto slot = static_cast<std::size_t>(index);
    if (table.isRead[slot]) {
        return Fail(ErrorCode::InvalidData, std::format("start slot {} is listed twice", index));
    }
    if (!stored.position || !stored.axes) {
        return Fail(ErrorCode::InvalidData,
                    std::format("start slot {} is missing its position or axes", index));
    }
    const auto& [forward, up] = *stored.axes;
    table.slots[slot] = {.position = *stored.position, .forward = forward, .up = up};
    table.isRead[slot] = true;
    return {};
}

}

Result<StartGridAsset> ParseSpb(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    Result<std::size_t> count =
        ExpectField(reader, kListField).and_then([&] { return reader.ReadListHeader(); });
    if (!count) {
        return std::unexpected(std::move(count.error()));
    }
    if (*count > kMaxSlots) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} start slots, wanted at most {}", *count, kMaxSlots));
    }
    SlotTable table{.slots = std::vector<StartSlot>(*count), .isRead = std::vector<bool>(*count)};
    const auto readSlot = [&] {
        StoredSlot stored;
        std::int32_t index = 0;
        return ExpectField(reader, kListField)
            .and_then([&] { return ReadValue(reader, index); })
            .and_then([&] { return ReadBlock<StoredSlot>(reader, kSlotFields, stored); })
            .and_then([&] { return Place(stored, index, table); });
    };
    for (std::size_t i = 0; i < *count; ++i) {
        Result<void> read = readSlot();
        if (!read) {
            return std::unexpected(std::move(read.error()));
        }
    }
    return reader.Expect(TokenKind::CloseBrace)
        .and_then([&](const Token&) { return reader.Expect(TokenKind::End); })
        .transform([&](const Token&) { return StartGridAsset{.slots = std::move(table.slots)}; });
}

}
