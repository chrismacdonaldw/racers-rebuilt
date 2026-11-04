#pragma once

#include <compare>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace rr::mods {

struct FileStamp {
    std::string path;
    std::uintmax_t size = 0;

    auto operator<=>(const FileStamp&) const = default;
};

/// Every file under `folder`, as lowercase `/` paths relative to it, sorted.
[[nodiscard]] std::vector<FileStamp> FileStamps(const std::filesystem::path& folder);

[[nodiscard]] std::uint64_t HashStamps(std::span<const FileStamp> stamps);

}
