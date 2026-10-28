#pragma once

#include <cstdint>
#include <filesystem>
#include <format>
#include <string_view>
#include <utility>

#include "core/result.hpp"

/// Thread-safe. Writes to stderr and, once opened, a file. Every line is flushed as it's written.
namespace rr::log {

enum class Level : std::uint8_t {
    Debug,
    Info,
    Warning,
    Error,
};

/// Truncates the file and creates missing parent folders. On failure, logging stays on stderr.
[[nodiscard]] Result<void> OpenFile(const std::filesystem::path& path);

void CloseFile();

void Write(Level level, std::string_view message);

template <typename... Args>
void Debug(std::format_string<Args...> format, Args&&... args) {
    Write(Level::Debug, std::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
void Info(std::format_string<Args...> format, Args&&... args) {
    Write(Level::Info, std::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
void Warn(std::format_string<Args...> format, Args&&... args) {
    Write(Level::Warning, std::format(format, std::forward<Args>(args)...));
}

template <typename... Args>
void Error(std::format_string<Args...> format, Args&&... args) {
    Write(Level::Error, std::format(format, std::forward<Args>(args)...));
}

}
