#include "app/screenshots.hpp"

#include <format>
#include <optional>
#include <system_error>
#include <utility>

#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "platform/image_file.hpp"

namespace rr::app {
namespace {

constexpr int kMaxScreenshots = 10000;

}

Result<std::filesystem::path> FreeScreenshotPath(const std::filesystem::path& folder) {
    for (int index = 0; index < kMaxScreenshots; ++index) {
        std::filesystem::path path = folder / std::format("LEGO{}.bmp", index);
        std::error_code error;
        if (!std::filesystem::exists(path, error) && !error) {
            return path;
        }
    }
    return Fail(ErrorCode::Io, std::format("{} already holds {} screenshots", Utf8FromPath(folder),
                                           kMaxScreenshots));
}

Result<std::filesystem::path> SaveScreenshot(const assets::ImageAsset& image,
                                             const std::filesystem::path& folder) {
    return FreeScreenshotPath(folder).and_then([&](std::filesystem::path path)
                                                   -> Result<std::filesystem::path> {
        return platform::SaveBmp(path, image.widthPx, image.heightPx, image.rgba).transform([&] {
            return std::move(path);
        });
    });
}

void SaveCapturedFrame(render::Renderer& renderer, const std::filesystem::path& folder) {
    const std::optional<assets::ImageAsset> frame = renderer.TakeCapture();
    if (!frame) {
        return;
    }
    const Result<std::filesystem::path> saved = SaveScreenshot(*frame, folder);
    if (saved) {
        log::Info("saved a screenshot to {}", Utf8FromPath(*saved));
    } else {
        log::Warn("{}", saved.error().message);
    }
}

}
