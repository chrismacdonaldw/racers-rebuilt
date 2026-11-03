#include "assets/vfs.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <ranges>
#include <utility>

#include "assets/game_path.hpp"
#include "core/assert.hpp"

namespace rr::assets {

void Vfs::Mount(std::unique_ptr<VfsLayer> layer) {
    RR_ASSERT(layer != nullptr);
    layers_.push_back(std::move(layer));
}

void Vfs::KeepLayers(std::size_t count) {
    if (count < layers_.size()) {
        layers_.resize(count);
    }
}

void Vfs::SetResolver(AssetResolver resolver) {
    resolver_ = std::move(resolver);
}

Result<FileData> Vfs::Read(std::string_view path) const {
    Result<std::string> normalized = NormalizeGamePath(path);
    if (!normalized) {
        return std::unexpected(std::move(normalized.error()));
    }
    if (!resolver_) {
        return ReadLayers(*normalized, path);
    }
    AssetRequest request{.path = *normalized, .generated = std::nullopt};
    resolver_(request);
    if (request.generated) {
        return FileData(std::move(*request.generated));
    }
    if (request.path == *normalized) {
        return ReadLayers(*normalized, path);
    }
    return NormalizeGamePath(request.path).and_then([&](const std::string& redirect) {
        return ReadLayers(redirect, std::format("{} (redirected to {})", path, redirect));
    });
}

Result<FileData> Vfs::ReadOriginal(std::string_view path) const {
    return NormalizeGamePath(path).and_then(
        [&](const std::string& normalized) { return ReadLayers(normalized, path); });
}

bool Vfs::Contains(std::string_view path) const {
    const Result<std::string> normalized = NormalizeGamePath(path);
    return normalized && std::ranges::any_of(layers_, [&](const std::unique_ptr<VfsLayer>& layer) {
               const std::optional<EntryInfo> entry = layer->Index().Find(*normalized);
               return entry && entry->kind == EntryKind::File;
           });
}

Result<std::optional<FileData>> Vfs::ReadLayer(std::size_t layer, std::string_view path) const {
    RR_ASSERT(layer < layers_.size());
    Result<std::string> normalized = NormalizeGamePath(path);
    if (!normalized) {
        return std::unexpected(std::move(normalized.error()));
    }
    const std::optional<EntryInfo> entry = layers_[layer]->Index().Find(*normalized);
    if (!entry || entry->kind != EntryKind::File) {
        return std::nullopt;
    }
    return layers_[layer]->Read(*entry).transform(
        [](FileData data) { return std::optional<FileData>(std::move(data)); });
}

Result<std::vector<FileData>> Vfs::ReadEveryLayer(std::string_view path) const {
    std::vector<FileData> files;
    for (std::size_t layer = 0; layer < layers_.size(); ++layer) {
        Result<std::optional<FileData>> data = ReadLayer(layer, path);
        if (!data) {
            return std::unexpected(std::move(data.error()));
        }
        if (*data) {
            files.push_back(std::move(**data));
        }
    }
    return files;
}

Result<FileData> Vfs::ReadLayers(std::string_view normalized, std::string_view shown) const {
    for (const std::unique_ptr<VfsLayer>& layer : layers_ | std::views::reverse) {
        const std::optional<EntryInfo> entry = layer->Index().Find(normalized);
        if (entry && entry->kind == EntryKind::File) {
            return layer->Read(*entry);
        }
    }
    return Fail(ErrorCode::NotFound, std::format("{} isn't in the game files or any mod", shown));
}

Result<std::vector<VfsEntry>> Vfs::List(std::string_view directory) const {
    Result<std::string> normalized = NormalizeGamePath(directory);
    if (!normalized) {
        return std::unexpected(std::move(normalized.error()));
    }
    std::vector<VfsEntry> children;
    bool isFound = false;
    for (const std::unique_ptr<VfsLayer>& layer : layers_ | std::views::reverse) {
        const std::optional<EntryInfo> entry = layer->Index().Find(*normalized);
        if (!entry || entry->kind != EntryKind::Directory) {
            continue;
        }
        isFound = true;
        for (const IndexEntry& child : layer->Index().Children(*normalized)) {
            children.push_back({.name = std::string(child.Name()), .kind = child.info.kind});
        }
    }
    if (!isFound) {
        return Fail(ErrorCode::NotFound,
                    std::format("directory {} isn't in the game files or any mod", directory));
    }

    std::ranges::stable_sort(children, {}, &VfsEntry::name);
    const auto duplicates = std::ranges::unique(children, {}, &VfsEntry::name);
    children.erase(duplicates.begin(), duplicates.end());
    return children;
}

bool HasFile(std::span<const VfsEntry> folder, std::string_view name) {
    return std::ranges::any_of(folder, [&](const VfsEntry& entry) {
        return entry.kind == EntryKind::File && ToUpperAscii(entry.name) == name;
    });
}

}
