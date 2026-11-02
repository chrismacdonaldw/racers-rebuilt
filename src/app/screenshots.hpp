#pragma once

#include <filesystem>

#include "assets/image_asset.hpp"
#include "core/result.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// The first `LEGO<n>.bmp` in `folder` that doesn't exist, trying `n` from 0 to 9999.
[[nodiscard]] Result<std::filesystem::path> FreeScreenshotPath(const std::filesystem::path& folder);

/// Returns the path it saved to.
[[nodiscard]] Result<std::filesystem::path> SaveScreenshot(const assets::ImageAsset& image,
                                                           const std::filesystem::path& folder);

/// Does nothing unless the renderer captured a frame, and logs instead of failing.
void SaveCapturedFrame(render::Renderer& renderer, const std::filesystem::path& folder);

}
