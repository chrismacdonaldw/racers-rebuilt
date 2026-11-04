#include "settings/registry.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <string>
#include <system_error>
#include <vector>

#include "core/assert.hpp"
#include "core/log.hpp"

namespace rr::settings {
namespace {

using Range = Registry::Range;

std::string_view TypeName(const SettingValue& value) {
    constexpr std::array<std::string_view, 4> kNames{"true or false", "a whole number", "a number",
                                                     "text"};
    return kNames[value.index()];
}

std::unexpected<Error> FailType(std::string_view key, const SettingValue& target) {
    return Fail(ErrorCode::InvalidData, std::format("{} must be {}", key, TypeName(target)));
}

bool IsChoice(const SettingValue& value, const std::vector<std::string>& choices) {
    const auto* text = std::get_if<std::string>(&value);
    return choices.empty() || (text != nullptr && std::ranges::contains(choices, *text));
}

std::unexpected<Error> FailChoice(std::string_view key, const std::vector<std::string>& choices) {
    std::string listed;
    for (const std::string& choice : choices) {
        listed += listed.empty() ? choice : ", " + choice;
    }
    return Fail(ErrorCode::InvalidData, std::format("{} must be one of {}", key, listed));
}

Result<SettingValue> Convert(std::string_view key, const SettingValue& target, SettingValue value) {
    if (value.index() == target.index()) {
        return value;
    }
    if (std::holds_alternative<double>(target) && std::holds_alternative<std::int64_t>(value)) {
        return static_cast<double>(std::get<std::int64_t>(value));
    }
    return FailType(key, target);
}

SettingValue Clamp(const SettingValue& value, const Range& range) {
    return std::visit(
        [&]<typename V>(const V& current) -> SettingValue {
            if constexpr (std::same_as<V, std::int64_t> || std::same_as<V, double>) {
                return std::clamp(current, std::get<V>(range.first), std::get<V>(range.second));
            } else {
                return current;
            }
        },
        value);
}

template <typename Number>
std::optional<Number> ParseNumber(std::string_view text) {
    Number number{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (error != std::errc() || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return number;
}

std::optional<SettingValue> ParseAs(const SettingValue& target, std::string_view text) {
    return std::visit(
        [&]<typename V>(const V&) -> std::optional<SettingValue> {
            if constexpr (std::same_as<V, bool>) {
                if (text == "true" || text == "false") {
                    return text == "true";
                }
                return std::nullopt;
            } else if constexpr (std::same_as<V, std::string>) {
                return std::string(text);
            } else {
                return ParseNumber<V>(text);
            }
        },
        target);
}

}

Result<void> Registry::Assign(std::string_view key, SettingValue value) {
    const auto declaration = declarations_.find(key);
    if (declaration == declarations_.end()) {
        Store(key, std::move(value));
        return {};
    }
    Result<SettingValue> converted =
        Convert(key, declaration->second.defaultValue, std::move(value));
    if (!converted) {
        return std::unexpected(std::move(converted.error()));
    }
    if (!IsChoice(*converted, declaration->second.choices)) {
        return FailChoice(key, declaration->second.choices);
    }
    if (declaration->second.range) {
        *converted = Clamp(*converted, *declaration->second.range);
    }
    Store(key, std::move(*converted));
    return {};
}

Result<void> Registry::AssignText(std::string_view key, std::string_view text) {
    const auto declaration = declarations_.find(key);
    if (declaration == declarations_.end()) {
        return Fail(ErrorCode::NotFound, std::format("there's no setting named {}", key));
    }
    std::optional<SettingValue> parsed = ParseAs(declaration->second.defaultValue, text);
    if (!parsed) {
        return FailType(key, declaration->second.defaultValue);
    }
    return Assign(key, std::move(*parsed));
}

bool Registry::IsAvailable(std::string_view key) const {
    const auto declaration = declarations_.find(key);
    if (declaration == declarations_.end() || !declaration->second.requirement) {
        return true;
    }
    const auto& [required, choice] = *declaration->second.requirement;
    const auto value = values_.find(required);
    if (value == values_.end()) {
        return false;
    }
    if (const auto* isOn = std::get_if<bool>(&value->second)) {
        return choice == (*isOn ? "true" : "false");
    }
    const auto* text = std::get_if<std::string>(&value->second);
    return text != nullptr && *text == choice;
}

void Registry::Observe(Observer observer) {
    observers_.push_back(std::move(observer));
}

void Registry::DeclareValue(std::string_view key, Declaration declaration) {
    RR_VERIFY(!declarations_.contains(key));
    const SettingValue defaultValue = declaration.defaultValue;
    declarations_.emplace(std::string(key), std::move(declaration));

    const auto existing = values_.find(key);
    if (existing == values_.end()) {
        values_.emplace(std::string(key), defaultValue);
        return;
    }
    SettingValue loaded = std::exchange(existing->second, defaultValue);
    Result<void> assigned = Assign(key, std::move(loaded));
    if (!assigned) {
        log::Warn("{}, using the default", assigned.error().message);
    }
}

void Registry::SetDeclared(std::string_view key, SettingValue value) {
    RR_VERIFY(declarations_.contains(key));
    const Result<void> assigned = Assign(key, std::move(value));
    RR_VERIFY(assigned.has_value());
}

void Registry::Store(std::string_view key, SettingValue value) {
    const auto existing = values_.find(key);
    if (existing != values_.end() && existing->second == value) {
        return;
    }
    values_.insert_or_assign(std::string(key), std::move(value));
    for (const Observer& observer : observers_) {
        observer(key);
    }
}

const SettingValue& Registry::DeclaredValue(std::string_view key) const {
    const auto value = values_.find(key);
    RR_VERIFY(value != values_.end() && declarations_.contains(key));
    return value->second;
}

}
