#include "assets/path_index.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <tuple>
#include <utility>

namespace rr::assets {
namespace {

using Key = std::tuple<std::string_view, std::string_view>;

std::size_t NameStart(std::string_view path) {
    const std::size_t separator = path.rfind('/');
    return separator == std::string_view::npos ? 0 : separator + 1;
}

Key KeyOf(std::string_view path) {
    const std::size_t nameStart = NameStart(path);
    return {path.substr(0, nameStart == 0 ? 0 : nameStart - 1), path.substr(nameStart)};
}

Key EntryKey(const IndexEntry& entry) {
    return KeyOf(entry.path);
}

}

std::string_view IndexEntry::Parent() const {
    return std::get<0>(KeyOf(path));
}

std::string_view IndexEntry::Name() const {
    return std::get<1>(KeyOf(path));
}

Result<PathIndex> PathIndex::Build(std::vector<IndexEntry> entries) {
    std::ranges::sort(entries, {}, EntryKey);
    const auto duplicate = std::ranges::adjacent_find(entries, {}, &IndexEntry::path);
    if (duplicate != entries.end()) {
        return Fail(ErrorCode::InvalidData, std::format("{} appears twice", duplicate->path));
    }
    return PathIndex(std::move(entries));
}

std::optional<EntryInfo> PathIndex::Find(std::string_view path) const {
    if (path.empty()) {
        return EntryInfo{.kind = EntryKind::Directory};
    }
    const auto entry = std::ranges::lower_bound(entries_, KeyOf(path), {}, EntryKey);
    if (entry == entries_.end() || entry->path != path) {
        return std::nullopt;
    }
    return entry->info;
}

std::span<const IndexEntry> PathIndex::Children(std::string_view directory) const {
    const auto children = std::ranges::equal_range(entries_, directory, {}, &IndexEntry::Parent);
    return {children.begin(), children.end()};
}

PathIndex::PathIndex(std::vector<IndexEntry> entries) : entries_(std::move(entries)) {}

}
