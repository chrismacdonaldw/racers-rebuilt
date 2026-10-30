#include "app/menu/menu_files.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

void CheckMainMenuFiles(const MenuFiles& files) {
    CHECK(files.layout.elements.size() == 11);
    CHECK(files.FindGameButtonStyle("nubutton") != nullptr);
    const assets::ImageStyle* racers = files.FindImageStyle("racers");
    REQUIRE(racers != nullptr);
    CHECK(racers->image == "racers");
}

void CheckImagesFound(const MenuFiles& files) {
    for (const char* image : {"backdrp", "RACERS", "clear32"}) {
        CHECK_MESSAGE(files.FindImage(image) != nullptr, image);
    }
    CHECK(files.FindImage("missing") == nullptr);
}

TEST_CASE("the retail main menu finds its styles and images") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<MenuFiles> files = ReadMenuFiles(*vfs, "MAINMENU");

    REQUIRE(files.has_value());
    CheckMainMenuFiles(*files);
    CheckImagesFound(*files);
}

void CheckButtonLabels(const std::vector<std::u16string>& lines) {
    REQUIRE(lines.size() > 39);
    CHECK(lines[37] == u"BUILD");
    CHECK(lines[33] == u"CIRCUIT RACE");
    CHECK(lines[39] == u"QUIT");
}

TEST_CASE("the retail menu text has the main menu's button labels") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<std::vector<std::u16string>> text = ReadMenuText(*vfs, "ENGLISH");

    REQUIRE(text.has_value());
    CheckButtonLabels(*text);
}

TEST_CASE("a menu screen that doesn't exist fails") {
    const assets::Vfs vfs;

    CHECK(ReadMenuFiles(vfs, "MAINMENU").error().code == ErrorCode::NotFound);
}

}
}
