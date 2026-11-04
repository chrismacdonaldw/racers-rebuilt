#pragma once

#include <cstddef>
#include <filesystem>

#include "assets/vfs.hpp"
#include "core/result.hpp"

namespace rr::app {

/// Models that can't be exported are skipped. Only a failure to write stops the export.
[[nodiscard]] Result<void> ExportGameModels(const assets::Vfs& vfs, std::size_t gameLayer,
                                            const std::filesystem::path& folder);

}
