#include "app/loop_sidecar.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <string>
#include <string_view>

#include "temp_folder.hpp"

namespace rr::app {
namespace {

Result<audio::LoopPoints> ReadText(const test::TempFolder& folder, std::string_view text) {
    const std::filesystem::path path = folder.Path() / "circuit1.toml";
    std::ofstream(path, std::ios::binary) << text;
    return ReadLoopSidecar(path);
}

TEST_CASE("a track with no sidecar loops whole") {
    const test::TempFolder folder;

    const Result<audio::LoopPoints> loop = ReadLoopSidecar(folder.Path() / "missing.toml");

    REQUIRE(loop.has_value());
    CHECK(loop->startFrame == 0);
    CHECK_FALSE(loop->endFrame.has_value());
}

TEST_CASE("a sidecar sets the loop start and end in samples") {
    const test::TempFolder folder;

    const Result<audio::LoopPoints> loop =
        ReadText(folder, "loop_start = 44100\nloop_end = 88200\n");

    REQUIRE(loop.has_value());
    CHECK(loop->startFrame == 44100);
    CHECK(loop->endFrame == 88200);
}

TEST_CASE("a sidecar with unusable loop points is an error") {
    const test::TempFolder folder;

    CHECK(ReadText(folder, "loop_start = 10\nloop_end = 10\n").error().message.find("after") !=
          std::string::npos);
    CHECK(ReadText(folder, "loop_start = -1\n").error().message.find("loop_start") !=
          std::string::npos);
    CHECK(ReadText(folder, "loop_end = \"soon\"\n").error().message.find("loop_end") !=
          std::string::npos);
    CHECK(ReadText(folder, "loop_start = \n").error().message.find("line 1") != std::string::npos);
}

}
}
