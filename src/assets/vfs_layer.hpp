#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <utility>

#include "assets/path_index.hpp"
#include "core/result.hpp"
#include "platform/mapped_file.hpp"

namespace rr::assets {

/// Bytes of one file. Borrowed bytes stay valid while the layer that lent them is alive.
class FileData {
public:
    explicit FileData(std::span<const std::byte> borrowed) : bytes_(borrowed) {}
    explicit FileData(platform::MappedFile owned)
        : owned_(std::move(owned)), bytes_(owned_->Bytes()) {}

    [[nodiscard]] std::span<const std::byte> Bytes() const { return bytes_; }

private:
    std::optional<platform::MappedFile> owned_;
    std::span<const std::byte> bytes_;
};

/// Const members must be thread-safe.
class VfsLayer {
public:
    virtual ~VfsLayer() = default;

    [[nodiscard]] virtual const PathIndex& Index() const = 0;

    [[nodiscard]] virtual Result<FileData> Read(EntryInfo file) const = 0;
};

}
