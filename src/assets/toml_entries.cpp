#include "assets/toml_entries.hpp"

namespace rr::assets {

std::optional<std::string> TomlFields::Text(std::string_view key) {
    const toml::node* node = Find(key);
    if (node == nullptr) {
        return std::nullopt;
    }
    std::optional<std::string> value = node->value_exact<std::string>();
    if (!value) {
        Complain(std::format("{} must be text", key));
    }
    return value;
}

std::optional<bool> TomlFields::Flag(std::string_view key) {
    const toml::node* node = Find(key);
    if (node == nullptr) {
        return std::nullopt;
    }
    const std::optional<bool> value = node->value_exact<bool>();
    if (!value) {
        Complain(std::format("{} must be true or false", key));
    }
    return value;
}

std::optional<std::int64_t> TomlFields::Number(std::string_view key, std::int64_t low,
                                               std::int64_t high) {
    const toml::node* node = Find(key);
    if (node == nullptr) {
        return std::nullopt;
    }
    const std::optional<std::int64_t> value = node->value_exact<std::int64_t>();
    if (!value || *value < low || *value > high) {
        Complain(std::format("{} must be a whole number from {} to {}", key, low, high));
        return std::nullopt;
    }
    return value;
}

std::optional<std::vector<std::string>> TomlFields::TextList(std::string_view key, std::size_t most,
                                                             std::string_view what) {
    const toml::node* node = Find(key);
    if (node == nullptr) {
        return std::nullopt;
    }
    const toml::array* items = node->as_array();
    if (items == nullptr || items->empty() || items->size() > most ||
        !items->is_homogeneous(toml::node_type::string)) {
        Complain(std::format("{} must list 1 to {} {}", key, most, what));
        return std::nullopt;
    }
    std::vector<std::string> list;
    items->for_each([&](const toml::value<std::string>& item) { list.push_back(item.get()); });
    return list;
}

void TomlFields::Complain(std::string problem) {
    if (!problem_) {
        problem_ = std::move(problem);
    }
}

Result<toml::table> ParseTomlText(std::string_view text, std::string_view sourceName) {
    toml::parse_result parsed = toml::parse(text, sourceName);
    if (!parsed) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} line {}: {}", sourceName, parsed.error().source().begin.line,
                                parsed.error().description()));
    }
    return std::move(parsed).table();
}

}
