#pragma once

#include <cstdint>
#include <filesystem>
#include <format>
#include <string_view>
#include <utility>

#include "core/result.hpp"

/// Process-wide log. Messages go to stderr and, once opened, to a log file.
///
/// Every function is thread-safe. Each line is flushed as it's written, so a crash
/// keeps everything logged before it.
namespace rr::log {

/// Severity of a log message.
enum class Level : std::uint8_t {
    Debug,
    Info,
    Warning,
    Error,
};

/// Opens `path` as the log file, truncating it and creating missing parent folders.
///
/// Replaces any file opened earlier. On failure, logging continues to stderr only.
[[nodiscard]] Result<void> OpenFile(const std::filesystem::path& path);

/// Closes the log file. Later messages go to stderr only.
void CloseFile();

/// Writes one line at `level`, prefixed with the seconds since the first call into the log.
void Write(Level level, std::string_view message);

/// Formats and writes a debug message.
template <typename... Args>
void Debug(std::format_string<Args...> format, Args&&... args) {
    Write(Level::Debug, std::format(format, std::forward<Args>(args)...));
}

/// Formats and writes an info message.
template <typename... Args>
void Info(std::format_string<Args...> format, Args&&... args) {
    Write(Level::Info, std::format(format, std::forward<Args>(args)...));
}

/// Formats and writes a warning. Use it for fallbacks, and name what was used instead.
template <typename... Args>
void Warn(std::format_string<Args...> format, Args&&... args) {
    Write(Level::Warning, std::format(format, std::forward<Args>(args)...));
}

/// Formats and writes an error message.
template <typename... Args>
void Error(std::format_string<Args...> format, Args&&... args) {
    Write(Level::Error, std::format(format, std::forward<Args>(args)...));
}

}
