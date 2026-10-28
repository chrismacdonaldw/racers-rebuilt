#include "assets/folder_layer.hpp"

#include <format>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "assets/game_path.hpp"
#include "core/assert.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"

namespace rr::assets {
namespace {

using DirectoryIterator = std::filesystem::recursive_directory_iterator;

struct ScannedFolder {
    std::vector<IndexEntry> entries;
    std::vector<std::filesystem::path> files;
};

std::optional<EntryKind> KindOf(const std::filesystem::directory_entry& entry) {
    std::error_code error;
    if (entry.is_directory(error)) {
        return EntryKind::Directory;
    }
    if (entry.is_regular_file(error)) {
        return EntryKind::File;
    }
    return std::nullopt;
}

void AddEntry(ScannedFolder& folder, DirectoryIterator& iterator, const std::filesystem::path& root,
              ScanDepth depth) {
    const std::filesystem::path& diskPath = iterator->path();
    const std::optional<EntryKind> kind = KindOf(*iterator);
    if (depth == ScanDepth::TopLevelFiles) {
        iterator.disable_recursion_pending();
    }
    if (!kind || (depth == ScanDepth::TopLevelFiles && *kind == EntryKind::Directory)) {
        return;
    }
    Result<std::string> path = NormalizeGamePath(Utf8FromPath(diskPath.lexically_relative(root)));
    if (!path) {
        log::Warn("skipping {}: {}", Utf8FromPath(diskPath), path.error().message);
        iterator.disable_recursion_pending();
        return;
    }
    EntryInfo info{.kind = *kind};
    if (*kind == EntryKind::File) {
        info.slot = static_cast<std::uint32_t>(folder.files.size());
        folder.files.push_back(diskPath);
    }
    folder.entries.push_back({.path = std::move(*path), .info = info});
}

}

Result<FolderLayer> FolderLayer::Scan(const std::filesystem::path& root, ScanDepth depth) {
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) {
        return Fail(ErrorCode::NotFound,
                    std::format("folder {} doesn't exist", Utf8FromPath(root)));
    }

    ScannedFolder folder;
    for (DirectoryIterator iterator(root, error); !error && iterator != DirectoryIterator();
         iterator.increment(error)) {
        AddEntry(folder, iterator, root, depth);
    }
    if (error) {
        return Fail(ErrorCode::Io,
                    std::format("can't scan {}: {}", Utf8FromPath(root), error.message()));
    }

    Result<PathIndex> index = PathIndex::Build(std::move(folder.entries));
    if (!index) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}: {}", Utf8FromPath(root), index.error().message));
    }
    return FolderLayer(std::move(*index), std::move(folder.files));
}

Result<FileData> FolderLayer::Read(EntryInfo file) const {
    RR_ASSERT(file.kind == EntryKind::File && file.slot < files_.size());
    Result<platform::MappedFile> mapped = platform::MappedFile::Open(files_[file.slot]);
    if (!mapped) {
        return std::unexpected(std::move(mapped.error()));
    }
    return FileData(std::move(*mapped));
}

FolderLayer::FolderLayer(PathIndex index, std::vector<std::filesystem::path> files)
    : index_(std::move(index)), files_(std::move(files)) {}

}
