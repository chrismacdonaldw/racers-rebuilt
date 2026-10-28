#include "core/log.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "temp_folder.hpp"

namespace rr {
namespace {

std::string ReadFile(const std::filesystem::path& path) {
    const std::ifstream file(path);
    std::stringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

TEST_CASE("the log file receives formatted messages with their level") {
    const test::TempFolder folder;
    const std::filesystem::path logPath = folder.Path() / "logs" / "test.log";

    REQUIRE(log::OpenFile(logPath).has_value());
    log::Info("loaded {} files", 3);
    log::Warn("override {} failed, using original", "circuit1.ogg");
    log::CloseFile();

    const std::string contents = ReadFile(logPath);
    CHECK(contents.find("info: loaded 3 files\n") != std::string::npos);
    CHECK(contents.find("warning: override circuit1.ogg failed, using original\n") !=
          std::string::npos);
}

TEST_CASE("opening a log file truncates the previous run") {
    const test::TempFolder folder;
    const std::filesystem::path logPath = folder.Path() / "test.log";

    REQUIRE(log::OpenFile(logPath).has_value());
    log::Info("first run");
    REQUIRE(log::OpenFile(logPath).has_value());
    log::Info("second run");
    log::CloseFile();

    const std::string contents = ReadFile(logPath);
    CHECK(contents.find("first run") == std::string::npos);
    CHECK(contents.find("second run") != std::string::npos);
}

TEST_CASE("messages after closing the file don't reach it") {
    const test::TempFolder folder;
    const std::filesystem::path logPath = folder.Path() / "test.log";

    REQUIRE(log::OpenFile(logPath).has_value());
    log::CloseFile();
    log::Error("after close");

    CHECK(ReadFile(logPath).empty());
}

}
}
