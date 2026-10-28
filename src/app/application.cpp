#include "app/application.hpp"

#include <filesystem>
#include <utility>

#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "platform/data_folder.hpp"

namespace rr::app {
namespace {

constexpr const char* kWindowTitle = "racers-rebuilt";
constexpr int kWindowWidthPx = 1280;
constexpr int kWindowHeightPx = 720;

void OpenLogFile(const std::filesystem::path& dataFolder) {
    const std::filesystem::path logPath = dataFolder / "logs" / "racers-rebuilt.log";
    Result<void> opened = log::OpenFile(logPath);
    if (!opened) {
        log::Warn("{}, logging to stderr only", opened.error().message);
    }
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

    log::Info("window open");
    return Application(std::move(*sdl), std::move(*window));
}

void Application::Run() {
    while (!events_.IsQuitRequested()) {
        events_.Wait();
    }
    log::Info("quit requested, shutting down");
}

Application::Application(platform::SdlContext sdl, platform::Window window)
    : sdl_(std::move(sdl)), window_(std::move(window)) {}

}
