#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "assets/token_reader.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

[[nodiscard]] Result<void> ReadValue(TokenReader& reader, std::int32_t& value);

/// Fails outside 0 to 255.
[[nodiscard]] Result<void> ReadValue(TokenReader& reader, std::uint8_t& value);
[[nodiscard]] Result<void> ReadValue(TokenReader& reader, float& value);
[[nodiscard]] Result<void> ReadValue(TokenReader& reader, std::string& value);
[[nodiscard]] Result<void> ReadValue(TokenReader& reader, Vector3& value);
[[nodiscard]] Result<void> ReadFloats(TokenReader& reader, std::initializer_list<float*> values);

/// Reads nothing: a flag field's presence sets it.
[[nodiscard]] Result<void> ReadValue(TokenReader& reader, bool& value);

template <typename T, std::size_t N>
[[nodiscard]] Result<void> ReadValue(TokenReader& reader, std::array<T, N>& values) {
    for (T& value : values) {
        Result<void> read = ReadValue(reader, value);
        if (!read) {
            return read;
        }
    }
    return {};
}

template <typename T>
[[nodiscard]] Result<void> ReadValue(TokenReader& reader, std::optional<T>& value) {
    return ReadValue(reader, value.emplace());
}

[[nodiscard]] Result<void> ExpectField(TokenReader& reader, FieldId id);

/// Reads a `{ }` block, nested blocks included, and drops it.
[[nodiscard]] Result<void> SkipBlock(TokenReader& reader);

/// Reads `[ count ] {`, calls `readElement` count times, then reads `}`.
template <typename ReadElement>
[[nodiscard]] Result<void> ReadList(TokenReader& reader, ReadElement&& readElement) {
    Result<std::size_t> count = reader.ReadListHeader();
    if (!count) {
        return std::unexpected(std::move(count.error()));
    }
    for (std::size_t i = 0; i < *count; ++i) {
        Result<void> read = readElement();
        if (!read) {
            return read;
        }
    }
    return reader.Expect(TokenKind::CloseBrace).transform([](const Token&) {});
}

template <typename T>
[[nodiscard]] Result<void> ReadValue(TokenReader& reader, std::vector<T>& values) {
    values.clear();
    return ReadList(reader, [&] { return ReadValue(reader, values.emplace_back()); });
}

template <typename Target>
struct FieldRule {
    FieldId id = 0;
    Result<void> (*read)(TokenReader& reader, Target& target) = nullptr;
};

template <typename Member>
struct MemberPointer;

template <typename Value, typename Class>
struct MemberPointer<Value Class::*> {
    using Target = Class;
};

template <auto... Members, typename Target>
[[nodiscard]] Result<void> ReadMembers(TokenReader& reader, Target& target) {
    Result<void> read;
    ((read = read.and_then([&] { return ReadValue(reader, target.*Members); })), ...);
    return read;
}

/// The members are read in order, each by the `ReadValue` overload for its type.
template <auto Member, auto... More>
[[nodiscard]] constexpr auto Field(FieldId id) {
    using Target = MemberPointer<decltype(Member)>::Target;
    return FieldRule<Target>{
        .id = id,
        .read = [](TokenReader& reader,
                   Target& target) { return ReadMembers<Member, More...>(reader, target); },
    };
}

template <auto Outer, auto Inner>
[[nodiscard]] constexpr auto NestedField(FieldId id) {
    using Target = MemberPointer<decltype(Outer)>::Target;
    return FieldRule<Target>{
        .id = id,
        .read = [](TokenReader& reader,
                   Target& target) { return ReadValue(reader, (target.*Outer).*Inner); },
    };
}

/// Reads one value of each type and drops them.
template <typename Target, typename... Values>
[[nodiscard]] constexpr FieldRule<Target> IgnoredField(FieldId id) {
    return {
        .id = id,
        .read = [](TokenReader& reader, Target&) -> Result<void> {
            Result<void> read;
            ((read = read.and_then([&] {
                 Values value{};
                 return ReadValue(reader, value);
             })),
             ...);
            return read;
        },
    };
}

/// Reads no value.
template <auto Member, auto Value>
[[nodiscard]] constexpr auto FlagField(FieldId id) {
    using Target = MemberPointer<decltype(Member)>::Target;
    return FieldRule<Target>{
        .id = id,
        .read = [](TokenReader&, Target& target) -> Result<void> {
            target.*Member = Value;
            return {};
        },
    };
}

/// Unknown fields fail.
template <typename Target>
[[nodiscard]] Result<void> ReadField(TokenReader& reader,
                                     std::type_identity_t<std::span<const FieldRule<Target>>> rules,
                                     Target& target) {
    Result<Token> field = reader.Expect(TokenKind::Field);
    if (!field) {
        return std::unexpected(std::move(field.error()));
    }
    const auto rule = std::ranges::find(rules, field->field, &FieldRule<Target>::id);
    if (rule == rules.end()) {
        return Fail(ErrorCode::InvalidData, std::format("unknown field 0x{:02X} at offset {}",
                                                        field->field, field->offset));
    }
    return rule->read(reader, target);
}

/// Stops before `}` or the end of data. Unknown fields fail.
template <typename Target>
[[nodiscard]] Result<void> ReadFields(
    TokenReader& reader, std::type_identity_t<std::span<const FieldRule<Target>>> rules,
    Target& target) {
    while (true) {
        Result<Token> token = reader.Peek();
        if (!token) {
            return std::unexpected(std::move(token.error()));
        }
        if (token->kind == TokenKind::CloseBrace || token->kind == TokenKind::End) {
            return {};
        }
        Result<void> read = ReadField<Target>(reader, rules, target);
        if (!read) {
            return read;
        }
    }
}

template <typename Target>
[[nodiscard]] Result<void> ReadBlock(TokenReader& reader,
                                     std::type_identity_t<std::span<const FieldRule<Target>>> rules,
                                     Target& target) {
    return reader.Expect(TokenKind::OpenBrace)
        .and_then([&](const Token&) { return ReadFields<Target>(reader, rules, target); })
        .and_then([&] { return reader.Expect(TokenKind::CloseBrace); })
        .transform([](const Token&) {});
}

/// Reads a whole file of `field [ count ] { field "name" { fields } ... }`.
template <typename Entry>
[[nodiscard]] Result<std::vector<Entry>> ParseNamedLibrary(
    std::span<const std::byte> bytes, FieldId field,
    std::type_identity_t<std::span<const FieldRule<Entry>>> rules) {
    TokenReader reader(bytes);
    std::vector<Entry> entries;
    const auto readEntry = [&] {
        Entry& entry = entries.emplace_back();
        return ExpectField(reader, field)
            .and_then([&] { return reader.ReadString(); })
            .and_then([&](std::string_view name) {
                entry.name = name;
                return ReadBlock<Entry>(reader, rules, entry);
            });
    };
    return ExpectField(reader, field)
        .and_then([&] { return ReadList(reader, readEntry); })
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .transform([&](const Token&) { return std::move(entries); });
}

}
