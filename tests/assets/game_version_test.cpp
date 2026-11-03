#include "assets/game_version.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <vector>

#include "assets/install.hpp"
#include "install_fixture.hpp"
#include "temp_folder.hpp"

namespace rr::assets {
namespace {

GameVersion ReleaseOf(const std::vector<test::JamFile>& files) {
    const test::TempFolder folder;
    std::ofstream(folder.Path() / "LEGO.JAM", std::ios::binary)
        << test::AsText(test::JamBuilder::Build(files, {"GAMEDATA", "MENUDATA"}));
    Result<JamArchive> jam = OpenInstall(folder.Path());
    REQUIRE(jam.has_value());
    return IdentifyRelease(*jam);
}

TEST_CASE("menu names with the console screens identify the 2001 release") {
    CHECK(ReleaseOf(test::ReleaseFiles(GameVersion::Release2001)) == GameVersion::Release2001);
    CHECK(ReleaseOf(test::ReleaseFiles(GameVersion::Release1999)) == GameVersion::Release1999);
}

TEST_CASE("an archive with missing or unreadable menu names counts as 1999") {
    CHECK(ReleaseOf({}) == GameVersion::Release1999);
    CHECK(ReleaseOf({{.path = "MENUDATA/MENUNAME.SRF", .contents = "\x05"}}) ==
          GameVersion::Release1999);
}

TEST_CASE("version names round trip and unknown names are rejected") {
    for (const GameVersion version : {GameVersion::Release1999, GameVersion::Release2001}) {
        CHECK(ParseGameVersion(NameOf(version)) == version);
    }
    CHECK_FALSE(ParseGameVersion("2000").has_value());
}

}
}
