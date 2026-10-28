#pragma once

#include <filesystem>

#include "assets/jam.hpp"
#include "assets/vfs_layer.hpp"
#include "platform/mapped_file.hpp"

namespace rr::assets {

/// Reads borrow from the mapping and never fail.
class JamArchive final : public VfsLayer {
public:
    [[nodiscard]] static Result<JamArchive> Open(const std::filesystem::path& path);

    [[nodiscard]] const PathIndex& Index() const override { return index_.paths; }
    [[nodiscard]] Result<FileData> Read(EntryInfo file) const override;

private:
    JamArchive(platform::MappedFile file, JamIndex index);

    platform::MappedFile file_;
    JamIndex index_;
};

}
