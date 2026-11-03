#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/result.hpp"
#include "settings/setting_spec.hpp"

namespace rr::settings {

/// Current value of every setting, declared or not. Undeclared keys come from newer builds and
/// are kept as loaded so saving doesn't drop them. Main thread only.
// NOLINTNEXTLINE(bugprone-exception-escape)
class Registry {
public:
    using Observer = std::function<void(std::string_view key)>;
    using Values = std::map<std::string, SettingValue, std::less<>>;
    using Range = std::pair<SettingValue, SettingValue>;

    struct Declaration {
        SettingValue defaultValue;
        std::optional<Range> range;
        std::string label;
        std::string category;
        bool changesGameplay = false;
        std::vector<std::string> choices;
    };

    using Declarations = std::map<std::string, Declaration, std::less<>>;

    template <SettingType T>
    void Declare(const SettingSpec<T>& spec) {
        std::optional<Range> range;
        if (spec.range) {
            range.emplace(ToSettingValue(spec.range->min), ToSettingValue(spec.range->max));
        }
        DeclareValue(spec.key, {.defaultValue = ToSettingValue(spec.defaultValue),
                                .range = range,
                                .label = std::string(spec.label),
                                .category = std::string(spec.category),
                                .changesGameplay = spec.changesGameplay,
                                .choices = {spec.choices.begin(), spec.choices.end()}});
    }

    /// `spec` must be declared.
    template <SettingType T>
    [[nodiscard]] ValueType<T> Get(const SettingSpec<T>& spec) const {
        const SettingValue& value = DeclaredValue(spec.key);
        if constexpr (std::same_as<T, int>) {
            return static_cast<int>(std::get<std::int64_t>(value));
        } else {
            return std::get<ValueType<T>>(value);
        }
    }

    /// `spec` must be declared.
    template <SettingType T>
    void Set(const SettingSpec<T>& spec, const ValueType<T>& value) {
        SetDeclared(spec.key, ToSettingValue<T>(value));
    }

    /// Converts and clamps values for declared keys. Fails on a type or choice that doesn't fit.
    [[nodiscard]] Result<void> Assign(std::string_view key, SettingValue value);

    /// Parses `text` as the declared type of `key`, as `--set key=value` does.
    [[nodiscard]] Result<void> AssignText(std::string_view key, std::string_view text);

    /// Called after a value changes.
    void Observe(Observer observer);

    [[nodiscard]] const Values& AllValues() const { return values_; }
    [[nodiscard]] const Declarations& AllDeclarations() const { return declarations_; }

private:
    void DeclareValue(std::string_view key, Declaration declaration);
    void SetDeclared(std::string_view key, SettingValue value);
    void Store(std::string_view key, SettingValue value);
    [[nodiscard]] const SettingValue& DeclaredValue(std::string_view key) const;

    Declarations declarations_;
    Values values_;
    std::vector<Observer> observers_;
};

}
