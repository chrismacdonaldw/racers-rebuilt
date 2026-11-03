#include "mods/override_folder.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "assets/folder_layer.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"

namespace rr::mods {
namespace {

constexpr std::uint64_t kFnvOffset = 0xCBF29CE484222325ULL;
constexpr std::uint64_t kFnvPrime = 0x100000001B3ULL;

struct FileStamp {
    std::string path;
    std::uintmax_t size = 0;

    auto operator<=>(const FileStamp&) const = default;
};

std::string GamePathKey(const std::filesystem::path& relative) {
    std::string key = Utf8FromPath(relative);
    std::ranges::transform(key, key.begin(), [](char c) {
        if (c == '\\') {
            return '/';
        }
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    });
    return key;
}

std::vector<FileStamp> FileStamps(const std::filesystem::path& folder) {
    std::vector<FileStamp> stamps;
    using DirectoryIterator = std::filesystem::recursive_directory_iterator;
    std::error_code error;
    for (DirectoryIterator entry(folder, error); !error && entry != DirectoryIterator();
         entry.increment(error)) {
        std::error_code kindError;
        if (!entry->is_regular_file(kindError)) {
            continue;
        }
        std::error_code sizeError;
        const std::uintmax_t size = entry->file_size(sizeError);
        stamps.push_back({.path = GamePathKey(entry->path().lexically_relative(folder)),
                          .size = sizeError ? 0 : size});
    }
    std::ranges::sort(stamps);
    return stamps;
}

std::uint64_t Mix(std::uint64_t hash, std::uint64_t byte) {
    return (hash ^ byte) * kFnvPrime;
}

std::uint64_t HashStamps(const std::vector<FileStamp>& stamps) {
    std::uint64_t hash = kFnvOffset;
    for (const FileStamp& stamp : stamps) {
        for (const char c : stamp.path) {
            hash = Mix(hash, static_cast<unsigned char>(c));
        }
        hash = Mix(hash, 0);
        for (int shift = 0; shift < 64; shift += 8) {
            hash = Mix(hash, (stamp.size >> shift) & 0xFFU);
        }
    }
    return hash;
}

}

void OverrideFolder::Mount(assets::Vfs& vfs, const settings::Registry& registry) {
    hash_.reset();
    std::error_code error;
    if (!registry.Get(kOverridesSetting) || !std::filesystem::is_directory(folder_, error)) {
        return;
    }
    const std::vector<FileStamp> stamps = FileStamps(folder_);
    if (stamps.empty()) {
        return;
    }
    Result<assets::FolderLayer> layer = assets::FolderLayer::Scan(folder_);
    if (!layer) {
        log::Warn("overrides: {}, skipping them", layer.error().message);
        return;
    }
    vfs.Mount(std::make_unique<assets::FolderLayer>(std::move(*layer)));
    hash_ = HashStamps(stamps);
    log::Info("overrides: {} files mounted from {}", stamps.size(), Utf8FromPath(folder_));
}

}
