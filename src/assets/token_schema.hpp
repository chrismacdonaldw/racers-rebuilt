#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "assets/token_reader.hpp"
#include "core/result.hpp"

namespace rr::assets {

[[nodiscard]] Result<void> ReadValue(TokenReader& reader, std::int32_t& value);
[[nodiscard]] Result<void> ReadValue(TokenReader& reader, float& value);
[[nodiscard]] Result<void> ReadValue(TokenReader& reader, std::string& value);

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

/// The member is read by the `ReadValue` overload for its type.
template <auto Member>
[[nodiscard]] constexpr auto Field(FieldId id) {
    using Target = MemberPointer<decltype(Member)>::Target;
    return FieldRule<Target>{
        .id = id,
        .read = [](TokenReader& reader,
                   Target& target) { return ReadValue(reader, target.*Member); },
    };
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
        Result<Token> field = reader.Expect(TokenKind::Field);
        if (!field) {
            return std::unexpected(std::move(field.error()));
        }
        const auto rule = std::ranges::find(rules, field->field, &FieldRule<Target>::id);
        if (rule == rules.end()) {
            return Fail(ErrorCode::InvalidData, std::format("unknown field 0x{:02X} at offset {}",
                                                            field->field, field->offset));
        }
        Result<void> read = rule->read(reader, target);
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

}
