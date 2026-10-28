#include "mods/mod_manifest.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>

#include <toml++/toml.hpp>

namespace rr::mods {
namespace {

bool IsValidId(std::string_view id) {
    return !id.empty() && std::ranges::all_of(id, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}

class FieldReader {
public:
    explicit FieldReader(const toml::table& table) : table_(&table) {}

    template <typename T>
    T Get(std::string_view key, std::optional<T> fallback = std::nullopt) {
        const toml::node* node = table_->get(key);
        if (node == nullptr && fallback) {
            return *fallback;
        }
        const std::optional<T> value = node == nullptr ? std::nullopt : ValueOf<T>(*node);
        if (!value && !error_) {
            error_ = node == nullptr ? std::format("{} is missing", key)
                                     : std::format("{} must be {}", key, TypeName<T>());
        }
        return value.value_or(T{});
    }

    [[nodiscard]] const std::optional<std::string>& Problem() const { return error_; }

private:
    template <typename T>
    static std::optional<T> ValueOf(const toml::node& node) {
        if constexpr (std::same_as<T, int>) {
            const std::optional<std::int64_t> value = node.value_exact<std::int64_t>();
            if (value && *value >= std::numeric_limits<int>::min() &&
                *value <= std::numeric_limits<int>::max()) {
                return static_cast<int>(*value);
            }
            return std::nullopt;
        } else {
            return node.value_exact<T>();
        }
    }

    template <typename T>
    static std::string_view TypeName() {
        if constexpr (std::same_as<T, int>) {
            return "a whole number";
        } else if constexpr (std::same_as<T, bool>) {
            return "true or false";
        } else {
            return "text";
        }
    }

    const toml::table* table_;
    std::optional<std::string> error_;
};

Result<ModManifest> ReadManifest(const toml::table& table) {
    FieldReader fields(table);
    ModManifest manifest{.id = fields.Get<std::string>("id"),
                         .name = fields.Get<std::string>("name"),
                         .version = fields.Get<std::string>("version"),
                         .priority = fields.Get<int>("priority", 0),
                         .isEnabledByDefault = fields.Get<bool>("enabled", true)};
    if (fields.Problem()) {
        return Fail(ErrorCode::InvalidData, *fields.Problem());
    }
    if (!IsValidId(manifest.id)) {
        return Fail(ErrorCode::InvalidData,
                    std::format("id {} may only use a-z, 0-9, _ and -", manifest.id));
    }
    return manifest;
}

}

Result<ModManifest> ParseModManifest(std::string_view text, std::string_view sourceName) {
    toml::parse_result parsed = toml::parse(text, sourceName);
    if (!parsed) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} line {}: {}", sourceName, parsed.error().source().begin.line,
                                parsed.error().description()));
    }
    Result<ModManifest> manifest = ReadManifest(parsed.table());
    if (!manifest) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}: {}", sourceName, manifest.error().message));
    }
    return manifest;
}

}
