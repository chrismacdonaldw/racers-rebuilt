#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "assets/cutscene_timeline.hpp"
#include "assets/token_reader.hpp"
#include "assets/token_schema.hpp"
#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::size_t kMaxCutsceneNameLength = 8;

[[nodiscard]] inline Result<void> ReadIndex(TokenReader& reader, std::size_t& index) {
    return reader.ReadInt().and_then([&](std::int32_t read) -> Result<void> {
        if (read < 0) {
            return Fail(ErrorCode::InvalidData, std::format("index {} is negative", read));
        }
        index = static_cast<std::size_t>(read);
        return {};
    });
}

[[nodiscard]] inline Result<std::string> ReadName(TokenReader& reader) {
    return reader.ReadString().and_then([](std::string_view name) -> Result<std::string> {
        if (name.size() > kMaxCutsceneNameLength) {
            return Fail(ErrorCode::InvalidData,
                        std::format("name '{}' is longer than 8 characters", name));
        }
        return std::string(name);
    });
}

template <typename ReadElement>
[[nodiscard]] Result<void> ReadNonEmptyList(TokenReader& reader, ReadElement&& readElement) {
    return reader.ReadListHeader().and_then([&](std::size_t count) -> Result<void> {
        if (count == 0) {
            return Fail(ErrorCode::InvalidData, "a list is empty");
        }
        return ReadElements(reader, count, readElement);
    });
}

[[nodiscard]] inline Result<void> ReadNames(TokenReader& reader, std::vector<std::string>& names) {
    return ReadNonEmptyList(reader, [&] {
        return ReadName(reader).transform(
            [&](std::string name) { names.push_back(std::move(name)); });
    });
}

/// Reads `[ count ] { field "name" { fields } ... }`.
template <typename Entry>
[[nodiscard]] Result<void> ReadNamedEntries(
    TokenReader& reader, FieldId field,
    std::type_identity_t<std::span<const FieldRule<Entry>>> rules, std::vector<Entry>& entries,
    FieldRepeats repeats = FieldRepeats::Overwrite) {
    return ReadNonEmptyList(reader, [&] {
        Entry& entry = entries.emplace_back();
        return ExpectField(reader, field)
            .and_then([&] { return ReadName(reader); })
            .and_then([&](std::string name) {
                entry.name = std::move(name);
                return ReadBlock<Entry>(reader, rules, entry, repeats);
            });
    });
}

template <auto Member, bool IsJointed>
[[nodiscard]] constexpr auto EntityNameField(FieldId id) {
    using Target = MemberPointer<decltype(Member)>::Target;
    return FieldRule<Target>{.id = id, .read = [](TokenReader& reader, Target& target) {
                                 return ReadName(reader).transform([&](std::string name) {
                                     target.*Member = EntityByName{.name = std::move(name),
                                                                   .isJointed = IsJointed};
                                 });
                             }};
}

template <typename Target, FieldId Id>
[[nodiscard]] constexpr FieldRule<Target> UnsupportedField() {
    return {.id = Id, .read = [](TokenReader&, Target&) -> Result<void> {
                return Fail(ErrorCode::InvalidData,
                            std::format("field 0x{:02X} isn't used by retail cutscenes", Id));
            }};
}

[[nodiscard]] inline Result<void> UnknownSection(const Token& section) {
    return Fail(ErrorCode::InvalidData, std::format("unsupported section 0x{:02X} at offset {}",
                                                    section.field, section.offset));
}

/// Calls `readSection` after each top-level field. A field may lead one section only.
template <typename ReadSection>
[[nodiscard]] Result<void> ReadSections(std::span<const std::byte> bytes,
                                        ReadSection&& readSection) {
    TokenReader reader(bytes);
    std::vector<FieldId> seen;
    while (true) {
        Result<Token> section = reader.Next();
        if (!section) {
            return std::unexpected(std::move(section.error()));
        }
        if (section->kind == TokenKind::End) {
            return {};
        }
        if (section->kind != TokenKind::Field || std::ranges::contains(seen, section->field)) {
            return Fail(ErrorCode::InvalidData,
                        std::format("expected a new section at offset {}", section->offset));
        }
        seen.push_back(section->field);
        if (Result<void> read = readSection(reader, *section); !read) {
            return read;
        }
    }
}

}
