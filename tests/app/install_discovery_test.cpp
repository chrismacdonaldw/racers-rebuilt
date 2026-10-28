#include "app/install_discovery.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string>

#include "install_fixture.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

TEST_CASE("a stale registry entry falls through to the next candidate") {
    const test::TempFolder folder;
    std::filesystem::create_directories(folder.Path() / "empty");
    test::WriteInstall(folder.Path() / "installed");
    const std::array<platform::InstallCandidate, 3> candidates{{
        {.folder = folder.Path() / "deleted", .source = "uninstall registry entry"},
        {.folder = folder.Path() / "empty", .source = "App Paths registry entry"},
        {.folder = folder.Path() / "installed", .source = "default install folder"},
    }};

    std::optional<GameInstall> install = FirstValidInstall(candidates);

    CHECK((install && install->folder == folder.Path() / "installed"));
    CHECK((install && !install->wasPicked));
}

TEST_CASE("no valid candidate finds nothing") {
    const test::TempFolder folder;
    const std::array<platform::InstallCandidate, 1> candidates{{
        {.folder = folder.Path(), .source = "exe folder"},
    }};

    CHECK_FALSE(FirstValidInstall(candidates).has_value());
}

TEST_CASE("a --game-path without LEGO.JAM is an error that names the file") {
    const test::TempFolder folder;

    Result<GameInstall> install = DiscoverInstall({.commandLine = folder.Path()});

    REQUIRE_FALSE(install.has_value());
    CHECK(install.error().message.find("--game-path") != std::string::npos);
    CHECK(install.error().message.find("LEGO.JAM") != std::string::npos);
}

TEST_CASE("a valid --game-path is used before the saved setting") {
    const test::TempFolder folder;
    test::WriteInstall(folder.Path() / "command line");
    test::WriteInstall(folder.Path() / "setting");

    Result<GameInstall> install = DiscoverInstall(
        {.commandLine = folder.Path() / "command line", .setting = folder.Path() / "setting"});

    REQUIRE(install.has_value());
    CHECK(install->folder == folder.Path() / "command line");
}

TEST_CASE("a valid game.path setting is found with no prompt") {
    const test::TempFolder folder;
    test::WriteInstall(folder.Path());

    Result<GameInstall> install = DiscoverInstall({.setting = folder.Path()});

    REQUIRE(install.has_value());
    CHECK(install->folder == folder.Path());
}

}
}
