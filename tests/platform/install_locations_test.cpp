#include "platform/install_locations.hpp"

#include <SDL3/SDL_stdinc.h>
#include <doctest/doctest.h>

#include <algorithm>
#include <string_view>

#include "core/utf8_path.hpp"

namespace rr::platform {
namespace {

constexpr std::wstring_view kQuotedCommand =
    LR"(C:\WINDOWS\IsUninst.exe -f"C:\LEGO Racers\Uninst.isu")";
constexpr std::wstring_view kBareCommand = LR"(IsUninst.exe -fD:\Racers\Uninst.isu -a)";
constexpr std::wstring_view kHyphenatedCommand =
    LR"(C:\my-files\IsUninst.exe -f"C:\LEGO Racers\Uninst.isu")";
constexpr std::wstring_view kNoFileCommand = LR"(C:\WINDOWS\IsUninst.exe)";
constexpr std::wstring_view kEmptyFileCommand = LR"(IsUninst.exe -f"")";

TEST_CASE("the install folder comes from the uninstall command's -f argument") {
    const std::filesystem::path quotedFolder(LR"(C:\LEGO Racers)");
    const std::filesystem::path bareFolder(LR"(D:\Racers)");

    CHECK(FolderFromUninstallCommand(kQuotedCommand) == quotedFolder);
    CHECK(FolderFromUninstallCommand(kBareCommand) == bareFolder);
    CHECK(FolderFromUninstallCommand(kHyphenatedCommand) == quotedFolder);
}

TEST_CASE("an uninstall command without a -f file has no folder") {
    CHECK_FALSE(FolderFromUninstallCommand(kNoFileCommand).has_value());
    CHECK_FALSE(FolderFromUninstallCommand(kEmptyFileCommand).has_value());
}

TEST_CASE("the default install folder under Program Files (x86) is checked first") {
    const char* programFiles = SDL_getenv("ProgramFiles(x86)");
    REQUIRE(programFiles != nullptr);
    const std::filesystem::path handPicked(LR"(C:\Games\LEGO Racers)");

    const std::vector<InstallCandidate> candidates = KnownInstallCandidates();

    REQUIRE_FALSE(candidates.empty());
    CHECK(candidates.front().folder ==
          PathFromUtf8(programFiles) / "LEGO Media" / "Games" / "LEGO Racers");
    CHECK(std::ranges::any_of(candidates, [&](const InstallCandidate& candidate) {
        return candidate.folder == handPicked;
    }));
}

}
}
