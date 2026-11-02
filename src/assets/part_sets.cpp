#include "assets/part_sets.hpp"

#include <array>
#include <optional>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kColourListField = 0x2D;
constexpr FieldId kSetListField = 0x2F;

struct StoredPartSet {
    std::optional<std::uint16_t> chassisType;
    std::optional<std::string> chassisName;
    std::vector<PartSetChoice> choices;
};

Result<void> ReadChoices(TokenReader& reader, StoredPartSet& set) {
    set.choices.clear();
    return ReadList(reader, [&] {
        return ReadMembers<&PartSetChoice::piece, &PartSetChoice::colour>(
            reader, set.choices.emplace_back());
    });
}

constexpr std::array kPartSetFields{
    Field<&StoredPartSet::chassisType>(0x2E),
    FieldRule<StoredPartSet>{.id = 0x30, .read = ReadChoices},
    Field<&StoredPartSet::chassisName>(0x31),
};

Result<std::vector<std::string>> ParseNameList(std::span<const std::byte> bytes, FieldId field) {
    TokenReader reader(bytes);
    std::vector<std::string> names;
    return ExpectField(reader, field)
        .and_then([&] { return ReadValue(reader, names); })
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .transform([&](const Token&) { return std::move(names); });
}

}

Result<std::vector<std::string>> ParsePieceColours(std::span<const std::byte> bytes) {
    return ParseNameList(bytes, kColourListField);
}

Result<std::vector<std::string>> ParsePartSetList(std::span<const std::byte> bytes) {
    return ParseNameList(bytes, kSetListField);
}

Result<PartSet> ParsePartSet(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    StoredPartSet stored;
    return ReadFields<StoredPartSet>(reader, kPartSetFields, stored, FieldRepeats::Fail)
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .and_then([&](const Token&) -> Result<PartSet> {
            if (!stored.chassisType || !stored.chassisName) {
                return Fail(ErrorCode::InvalidData, "a part set is missing its chassis");
            }
            return PartSet{.chassisType = *stored.chassisType,
                           .chassisName = std::move(*stored.chassisName),
                           .choices = std::move(stored.choices)};
        });
}

}
