#include "app/application.hpp"

#include <cmath>
#include <filesystem>
#include <utility>

#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "platform/clock.hpp"
#include "platform/data_folder.hpp"

namespace rr::app {
namespace {

constexpr const char* kWindowTitle = "racers-rebuilt";
constexpr int kWindowWidthPx = 1280;
constexpr int kWindowHeightPx = 720;
constexpr float kFallbackRefreshRateHz = 60.0F;

void OpenLogFile(const std::filesystem::path& dataFolder) {
    const std::filesystem::path logPath = dataFolder / "logs" / "racers-rebuilt.log";
    Result<void> opened = log::OpenFile(logPath);
    if (!opened) {
        log::Warn("{}, logging to stderr only", opened.error().message);
    }
}

platform::PacingConfig PacingForDisplay(const platform::Window& window) {
    const float refreshRateHz = window.RefreshRateHz().value_or(kFallbackRefreshRateHz);
    return {.mode = platform::PresentMode::Capped,
            .fpsCap = static_cast<int>(std::lround(refreshRateHz))};
}

}

Result<Application> Application::Create() {
    Result<std::filesystem::path> dataFolder = platform::ResolveDataFolder();
    if (!dataFolder) {
        return std::unexpected(std::move(dataFolder.error()));
    }
    OpenLogFile(*dataFolder);
    log::Info("racers-rebuilt {} starting, data folder {}", RR_VERSION, Utf8FromPath(*dataFolder));

    Result<platform::SdlContext> sdl = platform::SdlContext::Create();
    if (!sdl) {
        return std::unexpected(std::move(sdl.error()));
    }

    Result<platform::Window> window = platform::Window::Create(
        {.title = kWindowTitle, .widthPx = kWindowWidthPx, .heightPx = kWindowHeightPx});
    if (!window) {
        return std::unexpected(std::move(window.error()));
    }

    Result<platform::FrameTimer> timer = platform::FrameTimer::Create();
    if (!timer) {
        return std::unexpected(std::move(timer.error()));
    }
    const platform::PacingConfig pacing = PacingForDisplay(*window);
    log::Info("window open, pacing capped at {} FPS", pacing.fpsCap);

    return Application(std::move(*sdl), std::move(*window),
                       platform::FramePacer(std::move(*timer), pacing));
}

void Application::Run() {
    while (!events_.IsQuitRequested()) {
        events_.Poll();
        frameLoop_.RunFrame(platform::Now());
        pacer_.WaitForNextFrame();
    }
    log::Info("quit requested, shutting down");
}

Application::Application(platform::SdlContext sdl, platform::Window window,
                         platform::FramePacer pacer)
    : sdl_(std::move(sdl)), window_(std::move(window)), pacer_(std::move(pacer)) {}

}
