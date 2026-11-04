#pragma once

#include <cstddef>
#include <filesystem>
#include <span>

#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

/// Creates the folders on the way.
[[nodiscard]] Result<void> WriteExportFile(const std::filesystem::path& path,
                                           std::span<const std::byte> bytes);

/// Models that can't be exported are skipped. Only a failure to write stops the export.
[[nodiscard]] Result<void> ExportGameModels(const assets::Vfs& vfs, std::size_t gameLayer,
                                            const std::filesystem::path& folder);

}
