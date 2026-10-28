#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

#include "core/result.hpp"

namespace rr::platform {

/// `rgba` holds `widthPx * heightPx` RGBA8 pixels, top row first.
[[nodiscard]] Result<void> SavePng(const std::filesystem::path& path, std::uint32_t widthPx,
                                   std::uint32_t heightPx, std::span<const std::byte> rgba);

}
