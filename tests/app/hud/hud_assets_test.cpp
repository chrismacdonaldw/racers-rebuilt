#include "app/hud/hud_assets.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/image_list.hpp"

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

TEST_CASE("the retail hud image list holds the power-up panel in order") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }

    const Result<std::vector<assets::ImageListEntry>> images =
        vfs->Read("GAMEDATA/COMMON/LEGOIMGS.IDB").and_then([](const assets::FileData& file) {
            return assets::ParseImageList(file.Bytes());
        });

    REQUIRE(images.has_value());
    REQUIRE(images->size() == 24);
    const std::array<std::size_t, 5> panelImages{2, 11, 12, 13, 23};
    std::vector<std::string> names;
    names.reserve(panelImages.size());
    for (const std::size_t index : panelImages) {
        names.push_back((*images)[index].name);
    }
    CHECK(names == std::vector<std::string>{"cannon", "holder", "holderwp", "oneenh", "octan4"});
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
