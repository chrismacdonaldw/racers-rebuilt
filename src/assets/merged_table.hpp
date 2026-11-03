#pragma once

#include <algorithm>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// Later tables replace same-named entries in place, ignoring case, and append new names.
template <typename Entry>
[[nodiscard]] std::vector<Entry> MergeNamed(std::vector<std::vector<Entry>> tables) {
    if (tables.empty()) {
        return {};
    }
    std::vector<Entry> merged = std::move(tables.front());
    for (std::vector<Entry>& table : std::span(tables).subspan(1)) {
        for (Entry& entry : table) {
            const std::string name = ToUpperAscii(entry.name);
            const auto same = std::ranges::find_if(
                merged, [&](const Entry& kept) { return ToUpperAscii(kept.name) == name; });
            if (same == merged.end()) {
                merged.push_back(std::move(entry));
            } else {
                *same = std::move(entry);
            }
        }
    }
    return merged;
}

/// Fails when no layer has `path`, or when any layer's copy doesn't parse.
template <typename Entry>
[[nodiscard]] Result<std::vector<Entry>> ReadMergedTable(
    const Vfs& vfs, std::string_view path,
    Result<std::vector<Entry>> (*parse)(std::span<const std::byte>)) {
    Result<std::vector<FileData>> files = vfs.ReadEveryLayer(path);
    if (!files) {
        return std::unexpected(std::move(files.error()));
    }
    if (files->empty()) {
        return Fail(ErrorCode::NotFound,
                    std::format("{} isn't in the game files or any mod", path));
    }
    std::vector<std::vector<Entry>> tables;
    tables.reserve(files->size());
    for (const FileData& file : *files) {
        Result<std::vector<Entry>> table = parse(file.Bytes());
        if (!table) {
            return std::unexpected(std::move(table.error()));
        }
        tables.push_back(std::move(*table));
    }
    return MergeNamed(std::move(tables));
}

}
