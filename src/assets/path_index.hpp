#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"

namespace rr::assets {

enum class EntryKind : std::uint8_t {
    File,
    Directory,
};

struct EntryInfo {
    EntryKind kind = EntryKind::File;
    std::uint32_t slot = 0;
};

struct IndexEntry {
    std::string path;
    EntryInfo info;

    [[nodiscard]] std::string_view Parent() const;
    [[nodiscard]] std::string_view Name() const;
};

/// Entries keyed by normalized game path. The root directory is the empty path and always exists.
class PathIndex {
public:
    PathIndex() = default;

    [[nodiscard]] static Result<PathIndex> Build(std::vector<IndexEntry> entries);

    [[nodiscard]] std::optional<EntryInfo> Find(std::string_view path) const;
    [[nodiscard]] std::span<const IndexEntry> Children(std::string_view directory) const;
    [[nodiscard]] std::span<const IndexEntry> Entries() const { return entries_; }

private:
    explicit PathIndex(std::vector<IndexEntry> entries);

    std::vector<IndexEntry> entries_;
};

}
