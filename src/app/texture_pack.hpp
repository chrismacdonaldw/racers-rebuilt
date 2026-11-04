#pragma once

#include <cstddef>
#include <filesystem>

#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

/// Writes nothing when the folder name isn't a mod id or the folder already has a `mod.toml`.
[[nodiscard]] Result<void> DumpTexturePack(const assets::Vfs& vfs, std::size_t gameLayer,
                                           const std::filesystem::path& modFolder);

}
