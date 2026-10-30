#include "app/hud/hud_assets.hpp"

#include <doctest/doctest.h>

#include <optional>

#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

TEST_CASE("the retail english race text has the hud labels and driver names") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<HudLanguage> language = LoadHudLanguage(*vfs, "ENGLISH");

    REQUIRE(language.has_value());
    CHECK(language->strings.lap == u"LAP");
    CHECK(language->strings.go == u"GO");
    CHECK_FALSE(language->driverNames.empty());
}

TEST_CASE("a language without race text fails") {
    const assets::Vfs vfs;

    CHECK(LoadHudLanguage(vfs, "ENGLISH").error().code == ErrorCode::NotFound);
}

TEST_CASE("the hud needs both of its fonts") {
    UiFonts fonts{{"FONT_THS", UiFont{}}};

    CHECK_FALSE(FindHudFonts(fonts).has_value());

    fonts.emplace("IGNUM", UiFont{});
    const Result<HudFonts> found = FindHudFonts(fonts);

    REQUIRE(found.has_value());
    CHECK(found->text == &fonts.at("FONT_THS"));
    CHECK(found->numbers == &fonts.at("IGNUM"));
}

}
}
