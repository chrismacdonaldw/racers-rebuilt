#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

#include "app/install_discovery.hpp"
#include "core/result.hpp"

namespace rr::app {

inline constexpr std::uint32_t kExtractionVersion = 1;
inline constexpr std::string_view kExtractionStamp = "extracted.txt";

/// Copies `LEGO.JAM` and the top-level `.tun` and `.avi` files, replacing any older copy.
[[nodiscard]] Result<void> ExtractGameFiles(const std::filesystem::path& install,
                                            const std::filesystem::path& destination);

/// Empty unless `folder` holds a valid current copy taken from `gamePath`, when that is set.
[[nodiscard]] std::optional<GameInstall> OpenExtractedCopy(
    const std::filesystem::path& folder, const std::optional<std::filesystem::path>& gamePath);

}
