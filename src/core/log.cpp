#include "core/log.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <system_error>

#include "core/duration.hpp"
#include "core/utf8_path.hpp"

namespace rr::log {
namespace {

struct LogState {
    std::mutex mutex;
    std::ofstream file;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
};

LogState& State() {
    static LogState state;
    return state;
}

std::string_view LevelName(Level level) {
    switch (level) {
        case Level::Debug:
            return "debug";
        case Level::Info:
            return "info";
        case Level::Warning:
            return "warning";
        case Level::Error:
            return "error";
    }
    return "unknown";
}

}

Result<void> OpenFile(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return Fail(ErrorCode::Io, std::format("can't create log folder {}: {}",
                                               Utf8FromPath(path.parent_path()), error.message()));
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file) {
        return Fail(ErrorCode::Io, std::format("can't open log file {}", Utf8FromPath(path)));
    }

    LogState& state = State();
    const std::scoped_lock lock(state.mutex);
    state.file = std::move(file);
    return {};
}

void CloseFile() {
    LogState& state = State();
    const std::scoped_lock lock(state.mutex);
    state.file.close();
}

void Write(Level level, std::string_view message) {
    LogState& state = State();
    const FloatSeconds elapsed = std::chrono::steady_clock::now() - state.start;
    const std::string line =
        std::format("[{:9.3f}] {}: {}\n", elapsed.count(), LevelName(level), message);

    const std::scoped_lock lock(state.mutex);
    std::fputs(line.c_str(), stderr);
    if (state.file.is_open()) {
        state.file << line << std::flush;
    }
}

}
