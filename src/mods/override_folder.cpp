#include "mods/override_folder.hpp"

#include <memory>
#include <system_error>
#include <utility>
#include <vector>

#include "assets/folder_layer.hpp"
#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "mods/folder_hash.hpp"

namespace rr::mods {

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
