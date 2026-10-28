#pragma once

#include <cstdint>
#include <filesystem>
#include <format>
#include <random>
#include <system_error>

namespace rr::test {

/// Unique per test run. Deleted on destruction.
class TempFolder {
public:
    TempFolder() : path_(UniquePath()) { std::filesystem::create_directories(path_); }

    TempFolder(const TempFolder&) = delete;
    TempFolder(TempFolder&&) = delete;
    TempFolder& operator=(const TempFolder&) = delete;
    TempFolder& operator=(TempFolder&&) = delete;

    // NOLINTNEXTLINE(bugprone-exception-escape)
    ~TempFolder() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& Path() const { return path_; }

private:
    static std::filesystem::path UniquePath() {
        std::random_device random;
        const std::uint64_t id = (std::uint64_t{random()} << 32U) | random();
        return std::filesystem::temp_directory_path() / std::format("rr-test-{:016x}", id);
    }

    std::filesystem::path path_;
};

}
