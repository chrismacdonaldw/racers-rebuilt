#include "app/install_discovery.hpp"

#include <doctest/doctest.h>

#include <array>
#include <string>

#include "assets/install.hpp"
#include "core/utf8_path.hpp"
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

    std::optional<GameInstall> install =
        FirstValidInstall(candidates, assets::GameVersion::Release1999);

    CHECK((install && install->folder == folder.Path() / "installed"));
    CHECK((install && !install->wasPicked));
}

TEST_CASE("an install of another release falls through to the next candidate") {
    const test::TempFolder folder;
    test::WriteInstall(folder.Path() / "1999", assets::GameVersion::Release1999);
    test::WriteInstall(folder.Path() / "2001", assets::GameVersion::Release2001);
    const std::array<platform::InstallCandidate, 2> candidates{{
        {.folder = folder.Path() / "1999", .source = "game folder setting"},
        {.folder = folder.Path() / "2001", .source = "exe folder"},
    }};

    std::optional<GameInstall> install =
        FirstValidInstall(candidates, assets::GameVersion::Release2001);

    CHECK((install && install->folder == folder.Path() / "2001"));
}

TEST_CASE("with no install of the selected release, the first other release is taken") {
    const test::TempFolder folder;
    test::WriteInstall(folder.Path() / "a", assets::GameVersion::Release2001);
    test::WriteInstall(folder.Path() / "b", assets::GameVersion::Release2001);
    const std::array<platform::InstallCandidate, 3> candidates{{
        {.folder = folder.Path() / "deleted", .source = "game folder setting"},
        {.folder = folder.Path() / "a", .source = "exe folder"},
        {.folder = folder.Path() / "b", .source = "default install folder"},
    }};

    std::optional<GameInstall> install =
        FirstValidInstall(candidates, assets::GameVersion::Release1999);

    CHECK((install && install->folder == folder.Path() / "a"));
    CHECK((install && install->version == assets::GameVersion::Release2001));
}

TEST_CASE("a --game-path of another release is taken unless the version was set too") {
    const test::TempFolder folder;
    test::WriteInstall(folder.Path(), assets::GameVersion::Release2001);

    Result<GameInstall> adopted = DiscoverInstall({.commandLine = folder.Path()});
    Result<GameInstall> forced =
        DiscoverInstall({.commandLine = folder.Path(), .isVersionForced = true});

    CHECK((adopted && adopted->version == assets::GameVersion::Release2001));
    REQUIRE_FALSE(forced.has_value());
    CHECK(forced.error().message.find("holds the 2001 release, not 1999") != std::string::npos);
}

TEST_CASE("no valid candidate finds nothing") {
    const test::TempFolder folder;
    const std::array<platform::InstallCandidate, 1> candidates{{
        {.folder = folder.Path(), .source = "exe folder"},
    }};

    CHECK_FALSE(FirstValidInstall(candidates, assets::GameVersion::Release1999).has_value());
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

TEST_CASE("a rejected folder message names the folder, the problem and the right folder") {
    const test::TempFolder folder;
    Result<assets::JamArchive> jam = assets::OpenInstall(folder.Path());
    REQUIRE_FALSE(jam.has_value());

    const std::string message = RejectedFolderMessage(jam.error());

    CHECK(message.find(Utf8FromPath(folder.Path())) != std::string::npos);
    CHECK(message.find("Problem: " + jam.error().message) != std::string::npos);
    CHECK(message.find("LEGORacers.exe") != std::string::npos);
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
