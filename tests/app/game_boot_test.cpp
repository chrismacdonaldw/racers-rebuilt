#include "app/game_boot.hpp"

#include <doctest/doctest.h>

#include <string_view>
#include <utility>
#include <vector>

#include "app/game_settings.hpp"
#include "assets/install.hpp"
#include "core/utf8_path.hpp"
#include "install_fixture.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

GameInstall InstallOf(const std::filesystem::path& folder, assets::GameVersion version,
                      bool wasPicked) {
    test::WriteInstall(folder, version);
    Result<assets::JamArchive> jam = assets::OpenInstall(folder);
    REQUIRE(jam.has_value());
    return GameInstall{
        .folder = folder, .jam = std::move(*jam), .version = version, .wasPicked = wasPicked};
}

settings::Registry Declared() {
    settings::Registry registry;
    DeclareGameSettings(registry);
    return registry;
}

TEST_CASE("an install of the other release switches the version and records its folder") {
    const test::TempFolder folder;
    settings::Registry registry = Declared();

    const std::vector<std::string_view> changed = RecordInstall(
        registry, InstallOf(folder.Path(), assets::GameVersion::Release2001, false), false);

    CHECK(changed == std::vector<std::string_view>{kGameVersion.key, kGamePath2001.key});
    CHECK(registry.Get(kGameVersion) == "2001");
    CHECK(registry.Get(kGamePath2001) == Utf8FromPath(folder.Path()));
    CHECK(registry.Get(kGamePath).empty());
}

TEST_CASE("a --game-path of the other release switches the version but records no folder") {
    const test::TempFolder folder;
    settings::Registry registry = Declared();

    const std::vector<std::string_view> changed = RecordInstall(
        registry, InstallOf(folder.Path(), assets::GameVersion::Release2001, false), true);

    CHECK(changed == std::vector<std::string_view>{kGameVersion.key});
    CHECK(registry.Get(kGamePath2001).empty());
}

TEST_CASE("an install of the selected release changes only a picked folder") {
    const test::TempFolder folder;
    settings::Registry registry = Declared();

    CHECK(RecordInstall(registry,
                        InstallOf(folder.Path() / "found", assets::GameVersion::Release1999, false),
                        false)
              .empty());
    CHECK(RecordInstall(registry,
                        InstallOf(folder.Path() / "picked", assets::GameVersion::Release1999, true),
                        false) == std::vector<std::string_view>{kGamePath.key});
    CHECK(registry.Get(kGameVersion) == "1999");
}

}
}
