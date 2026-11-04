#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::assets {

[[nodiscard]] inline std::string_view TextOf(const FileData& file) {
    const std::span<const std::byte> bytes = file.Bytes();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

template <typename Entry>
struct ModTables {
    std::vector<Entry> entries;
    std::vector<std::string> problems;
};

/// Every layer's copy of `path`, lowest first. A copy that doesn't parse is left out as a problem.
template <typename Entry>
[[nodiscard]] Result<ModTables<Entry>> ReadModTables(
    const Vfs& vfs, std::string_view path,
    Result<std::vector<Entry>> (*parse)(std::string_view, std::string_view)) {
    Result<std::vector<FileData>> files = vfs.ReadEveryLayer(path);
    if (!files) {
        return std::unexpected(std::move(files.error()));
    }
    ModTables<Entry> tables;
    for (const FileData& file : *files) {
        Result<std::vector<Entry>> entries = parse(TextOf(file), path);
        if (entries) {
            std::ranges::move(*entries, std::back_inserter(tables.entries));
        } else {
            tables.problems.push_back(std::move(entries.error().message));
        }
    }
    return tables;
}

}
