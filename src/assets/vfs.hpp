#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
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

/// `path` arrives normalized. A resolver may redirect it, or fill `generated` to skip the layers.
struct AssetRequest {
    std::string path;
    std::optional<std::vector<std::byte>> generated;
};

using AssetResolver = std::function<void(AssetRequest&)>;

/// A path resolves to the highest layer that has it, ignoring case. Const members are thread-safe.
class Vfs {
public:
    /// Adds `layer` above every layer mounted so far.
    void Mount(std::unique_ptr<VfsLayer> layer);

    [[nodiscard]] std::size_t LayerCount() const { return layers_.size(); }

    /// Unmounts every layer above the first `count`. Nothing may still borrow bytes from them.
    void KeepLayers(std::size_t count);

    /// `resolver` runs on the reading thread before every read, so it must be thread-safe.
    void SetResolver(AssetResolver resolver);

    [[nodiscard]] Result<FileData> Read(std::string_view path) const;

    /// Skips the resolver.
    [[nodiscard]] Result<FileData> ReadOriginal(std::string_view path) const;

    /// Skips the resolver.
    [[nodiscard]] bool Contains(std::string_view path) const;

    /// Lowest layer first, skipping the resolver. Empty when no layer has `path`.
    [[nodiscard]] Result<std::vector<FileData>> ReadEveryLayer(std::string_view path) const;

    /// Merges every layer's children, sorted by name.
    [[nodiscard]] Result<std::vector<VfsEntry>> List(std::string_view directory) const;

private:
    [[nodiscard]] Result<FileData> ReadLayers(std::string_view normalized,
                                              std::string_view shown) const;

    std::vector<std::unique_ptr<VfsLayer>> layers_;
    AssetResolver resolver_;
};

/// `name` must be upper case.
[[nodiscard]] bool HasFile(std::span<const VfsEntry> folder, std::string_view name);

}
