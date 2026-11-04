#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/result.hpp"

namespace rr::assets {

class JsonValue;

using JsonArray = std::vector<JsonValue>;
using JsonObject = std::vector<std::pair<std::string, JsonValue>>;

// NOLINTNEXTLINE(misc-no-recursion)
class JsonValue {
public:
    using Data = std::variant<std::monostate, bool, double, std::string, JsonArray, JsonObject>;

    JsonValue() = default;
    explicit JsonValue(Data data) : data_(std::move(data)) {}

    /// Null when this isn't an object or has no `key`.
    [[nodiscard]] const JsonValue* Find(std::string_view key) const;

    [[nodiscard]] std::optional<double> Number() const;
    [[nodiscard]] std::optional<std::string_view> Text() const;
    /// Empty when this isn't an array.
    [[nodiscard]] std::span<const JsonValue> Items() const;

private:
    Data data_;
};

/// A `\u` escape below U+0100 reads as one Latin-1 byte, as `JsonString` writes it.
[[nodiscard]] Result<JsonValue> ParseJson(std::string_view text);

}
