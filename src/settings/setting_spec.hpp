#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace rr::settings {

template <typename T>
concept SettingType = std::same_as<T, bool> || std::same_as<T, int> || std::same_as<T, double> ||
                      std::same_as<T, std::string_view>;

using SettingValue = std::variant<bool, std::int64_t, double, std::string>;

template <SettingType T>
using ValueType = std::conditional_t<std::same_as<T, std::string_view>, std::string, T>;

template <SettingType T>
struct SettingRange {
    T min;
    T max;
};

/// `key` is a dotted TOML path. Out-of-range values are clamped. A `/` in `category` nests menus.
template <SettingType T>
struct SettingSpec {
    std::string_view key;
    T defaultValue{};
    std::optional<SettingRange<T>> range;
    std::string_view label;
    std::string_view category;
    bool changesGameplay = false;
};

template <SettingType T>
[[nodiscard]] SettingValue ToSettingValue(T value) {
    if constexpr (std::same_as<T, int>) {
        return std::int64_t{value};
    } else if constexpr (std::same_as<T, std::string_view>) {
        return std::string(value);
    } else {
        return value;
    }
}

}
