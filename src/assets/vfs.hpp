#pragma once

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "assets/path_index.hpp"
#include "assets/vfs_layer.hpp"
#include "core/result.hpp"

namespace rr::assets {

struct VfsEntry {
    std::string name;
    EntryKind kind = EntryKind::File;
};

/// A path resolves to the highest layer that has it, ignoring case. Const members are thread-safe.
class Vfs {
public:
    /// Adds `layer` above every layer mounted so far.
    void Mount(std::unique_ptr<VfsLayer> layer);

    [[nodiscard]] Result<FileData> Read(std::string_view path) const;

    /// Merges every layer's children, sorted by name.
    [[nodiscard]] Result<std::vector<VfsEntry>> List(std::string_view directory) const;

private:
    std::vector<std::unique_ptr<VfsLayer>> layers_;
};

/// `name` must be upper case.
[[nodiscard]] bool HasFile(std::span<const VfsEntry> folder, std::string_view name);

}
