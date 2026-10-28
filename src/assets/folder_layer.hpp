#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "assets/vfs_layer.hpp"

namespace rr::assets {

enum class ScanDepth : std::uint8_t { TopLevelFiles, Recursive };

class FolderLayer final : public VfsLayer {
public:
    /// Indexes `root` once, so later changes on disk aren't seen. Names that aren't valid game
    /// paths are skipped with a warning.
    [[nodiscard]] static Result<FolderLayer> Scan(const std::filesystem::path& root,
                                                  ScanDepth depth = ScanDepth::Recursive);

    [[nodiscard]] const PathIndex& Index() const override { return index_; }
    [[nodiscard]] Result<FileData> Read(EntryInfo file) const override;

private:
    FolderLayer(PathIndex index, std::vector<std::filesystem::path> files);

    PathIndex index_;
    std::vector<std::filesystem::path> files_;
};

}
