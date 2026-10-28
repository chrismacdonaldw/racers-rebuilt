#pragma once

#include <filesystem>

#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

/// Writes each BMP and TGA to `<folder>/<game path>.png`, skipping any that don't decode.
[[nodiscard]] Result<void> DumpImages(const assets::Vfs& vfs, const std::filesystem::path& folder);

}
