#include "app/menu/race_preview.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "app/menu/menu_files.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/menu/race_setup_menu.hpp"
#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

using assets::GameVersion;

const RacePreview kRocketRace{.circuit = 6, .position = 0};

bool IsRed(const render::Color& color) {
    return color.r > 0.8F && color.g < 0.2F && color.b < 0.2F;
}

bool IsYellow(const render::Color& color) {
    return color.r > 0.9F && color.g > 0.8F && color.b < 0.2F;
}

bool IsSame(const std::optional<render::Color>& x, const std::optional<render::Color>& y) {
    if (!x || !y) {
        return x.has_value() == y.has_value();
    }
    return x->r == y->r && x->g == y->g && x->b == y->b && x->a == y->a;
}

TEST_CASE("the Rocket Racer race's frame is yellow in 1999 and red in 2001") {
    const render::Color unset{.r = 0.0F, .g = 0.0F, .b = 0.0F, .a = 0.0F};
    const render::Color original =
        FrameTintOf(kRocketRace, sim::RulesFor(GameVersion::Release1999)).value_or(unset);
    const render::Color rerelease =
        FrameTintOf(kRocketRace, sim::RulesFor(GameVersion::Release2001)).value_or(unset);

    CHECK(IsYellow(original));
    CHECK(original.a == 1.0F);
    CHECK(IsRed(rerelease));
}

TEST_CASE("the other races' frames match in both versions") {
    const RacePreview pirates{.circuit = 0, .position = 0};
    const RacePreview lastRocketSlot{.circuit = 6, .position = 3};

    for (const GameVersion version : {GameVersion::Release1999, GameVersion::Release2001}) {
        const sim::VersionRules& rules = sim::RulesFor(version);
        CHECK(IsYellow(FrameTintOf(pirates, rules).value_or(render::Color{})));
        CHECK(IsSame(FrameTintOf(lastRocketSlot, rules),
                     FrameTintOf({.circuit = 0, .position = 1}, rules)));
    }
}

TEST_CASE("races past the original seven circuits keep the frame's own colour") {
    CHECK_FALSE(FrameTintOf({.circuit = 7}, {}).has_value());
}

TEST_CASE("only the retail Rocket Racer race changes frame colour with the version") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    std::vector<std::string> changed;
    for (const CourseChoice& track : LoadTrackChoices(*vfs).value_or({})) {
        const RacePreview preview = track.previews.front().value_or(RacePreview{});
        if (!IsSame(FrameTintOf(preview, sim::RulesFor(GameVersion::Release1999)),
                    FrameTintOf(preview, sim::RulesFor(GameVersion::Release2001)))) {
            changed.push_back(preview.frame);
        }
    }
    CHECK(changed == std::vector<std::string>{"Final"});
}

bool HasBorderFrame(const assets::Vfs& vfs, std::string_view layout) {
    const Result<const assets::MenuElement*> border =
        ReadMenuFiles(vfs, layout).and_then([&](const MenuFiles& files) {
            return FindMenuElement(files, layout, "brickbox");
        });
    return border && std::holds_alternative<assets::FrameElement>((*border)->body);
}

TEST_CASE("both retail race pick layouts frame the preview with brickbox") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    CHECK(HasBorderFrame(*vfs, "SINGRACE"));
    CHECK(HasBorderFrame(*vfs, "CIRCRACE"));
}

}
}
