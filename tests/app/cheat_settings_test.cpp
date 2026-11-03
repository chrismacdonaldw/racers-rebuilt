#include "app/cheat_settings.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <string_view>

#include "settings/registry.hpp"
#include "sim/race/cheats.hpp"

namespace rr::app {
namespace {

using sim::Cheat;
using sim::Cheats;

settings::Registry Toggled(std::initializer_list<Cheat> cheats) {
    settings::Registry registry;
    DeclareCheatSettings(registry);
    for (const CheatToggle& toggle : kCheatToggles) {
        registry.Set(toggle.setting, std::ranges::find(cheats, toggle.cheat) != cheats.end());
    }
    return registry;
}

Cheats Entered(std::initializer_list<std::string_view> plates) {
    Cheats cheats;
    for (const std::string_view plate : plates) {
        REQUIRE(cheats.EnterPlate(plate));
    }
    return cheats;
}

std::string KeyOf(Cheat cheat) {
    std::string key = "cheats.";
    for (const char c : sim::PlateOf(cheat)) {
        key += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return key;
}

TEST_CASE("each cheat toggle is keyed by its plate, in bit order") {
    unsigned previous = 0;
    for (const CheatToggle& toggle : kCheatToggles) {
        CHECK(toggle.setting.key == KeyOf(toggle.cheat));
        CHECK(toggle.setting.changesGameplay);
        CHECK(static_cast<unsigned>(toggle.cheat) > previous);
        previous = static_cast<unsigned>(toggle.cheat);
    }
}

TEST_CASE("toggled cheats enter in bit order, so the later plate's exclusions win") {
    const Cheats cheats =
        WithCheatToggles({}, Toggled({Cheat::YellowBricks, Cheat::RedBricks, Cheat::NoWheels}));

    CHECK(cheats == Entered({"PGLLRD", "PGLLYLL", "NWHLS"}));
    CHECK_FALSE(cheats.Has(Cheat::RedBricks));
}

TEST_CASE("toggles enter after the session's cheats, so a toggled plate's exclusions win") {
    const Cheats session = Entered({"PGLLRD", "FSTFRWRD"});

    CHECK(WithCheatToggles(session, Toggled({})) == session);
    CHECK(WithCheatToggles(session, Toggled({Cheat::RedBricks})) == session);
    CHECK(WithCheatToggles(session, Toggled({Cheat::GreenBricks})) ==
          Entered({"FSTFRWRD", "PGLLGRN"}));
}

TEST_CASE("a race keeps the cheats its scene names over the session's") {
    const Cheats named = Entered({"NDRVR"});
    const Cheats session = Entered({"MXPMX"});

    CHECK(RaceCheats(named, session) == named);
    CHECK(RaceCheats({}, session) == session);
}

}
}
