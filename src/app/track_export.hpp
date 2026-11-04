#pragma once

#include <filesystem>
#include <string_view>

#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

/// Writes `track.glb` and `track.toml` into `folder`, leaving out unreadable models with a warning.
[[nodiscard]] Result<void> ExportTrack(const assets::Vfs& vfs, std::string_view track,
                                       const std::filesystem::path& folder);

}
