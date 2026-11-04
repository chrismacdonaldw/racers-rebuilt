#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/result.hpp"

#include <toml++/toml.hpp>

namespace rr::assets {

/// Reads one table's keys, keeping the first problem.
class TomlFields {
public:
    explicit TomlFields(const toml::table& table) : table_(&table) {}

    [[nodiscard]] const toml::node* Find(std::string_view key) const { return table_->get(key); }

    std::optional<std::string> Text(std::string_view key);
    std::optional<bool> Flag(std::string_view key);
    std::optional<std::int64_t> Number(std::string_view key, std::int64_t low, std::int64_t high);
    std::optional<std::vector<std::string>> TextList(std::string_view key, std::size_t most,
                                                     std::string_view what);

    void Complain(std::string problem);

    [[nodiscard]] const std::optional<std::string>& Problem() const { return problem_; }

private:
    const toml::table* table_;
    std::optional<std::string> problem_;
};

[[nodiscard]] Result<toml::table> ParseTomlText(std::string_view text, std::string_view sourceName);

/// The `[[key]]` tables of `text`, each read by `parse`. Fails on the first that doesn't read.
template <typename Entry, typename Parse>
[[nodiscard]] Result<std::vector<Entry>> ParseTomlEntries(std::string_view text,
                                                          std::string_view sourceName,
                                                          std::string_view key, Parse parse) {
    const Result<toml::table> root = ParseTomlText(text, sourceName);
    if (!root) {
        return std::unexpected(root.error());
    }
    const toml::node* node = root->get(key);
    if (node == nullptr) {
        return std::vector<Entry>{};
    }
    const toml::array* list = node->as_array();
    if (list == nullptr || (!list->empty() && !list->is_homogeneous(toml::node_type::table))) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}: {} must be [[{}]] tables", sourceName, key, key));
    }
    std::vector<Entry> entries;
    for (std::size_t index = 0; index < list->size(); ++index) {
        Result<Entry> entry = parse(*list->get(index)->as_table());
        if (!entry) {
            return Fail(entry.error().code, std::format("{}: {} {}: {}", sourceName, key, index + 1,
                                                        entry.error().message));
        }
        entries.push_back(std::move(*entry));
    }
    return entries;
}

}
