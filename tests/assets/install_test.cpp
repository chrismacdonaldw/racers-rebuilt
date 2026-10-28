#include "assets/install.hpp"

#include <doctest/doctest.h>

#include <fstream>
#include <string>

#include "assets/jam_builder.hpp"
#include "install_fixture.hpp"
#include "temp_folder.hpp"

namespace rr::assets {
namespace {

TEST_CASE("a folder with a valid LEGO.JAM opens as an install") {
    const test::TempFolder folder;
    test::WriteInstall(folder.Path());

    CHECK(OpenInstall(folder.Path()).has_value());
}

TEST_CASE("a folder without LEGO.JAM is rejected with a message naming the file") {
    const test::TempFolder folder;

    Result<JamArchive> install = OpenInstall(folder.Path());

    REQUIRE_FALSE(install.has_value());
    CHECK(install.error().code == ErrorCode::NotFound);
    CHECK(install.error().message.find("LEGO.JAM") != std::string::npos);
}

TEST_CASE("a folder that doesn't exist is rejected") {
    const test::TempFolder folder;

    CHECK(OpenInstall(folder.Path() / "deleted").error().code == ErrorCode::NotFound);
}

TEST_CASE("a LEGO.JAM without MENUDATA is rejected") {
    const test::TempFolder folder;
    std::ofstream(folder.Path() / "LEGO.JAM", std::ios::binary)
        << test::AsText(test::JamBuilder::Build({{.path = "GAMEDATA/A.TXT", .contents = "a"}}));

    Result<JamArchive> install = OpenInstall(folder.Path());

    REQUIRE_FALSE(install.has_value());
    CHECK(install.error().message.find("MENUDATA") != std::string::npos);
}

TEST_CASE("installer language ids map to JAM language folders") {
    CHECK(LanguageForLangId(0x09) == "ENGLISH");
    CHECK(LanguageForLangId(0x040c) == "FRENCH");
    CHECK(LanguageForLangId(0x14) == "NORWEGI");
    CHECK_FALSE(LanguageForLangId(0x0411).has_value());
}

}
}
