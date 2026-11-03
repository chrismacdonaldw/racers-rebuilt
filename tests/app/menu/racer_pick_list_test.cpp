#include "app/menu/racer_pick_list.hpp"

#include <doctest/doctest.h>

#include <array>
#include <vector>

namespace rr::app {
namespace {

settings::Racer Named(const char* name, const char* chassis) {
    return {.name = name, .chassis = chassis};
}

TEST_CASE("the pick lists your racers first, then the default racers you don't hold") {
    const settings::RacerStore store{.racers = {Named("ME", "a"), Named("JOAN", "b")}};
    const std::array defaults{Named("JOAN", "b"), Named("JOAN", "c"), Named("SAM", "d")};

    const std::vector<settings::Racer> racers = PickableRacers(store, defaults);

    REQUIRE(racers.size() == 4);
    CHECK(racers[1].chassis == "b");
    CHECK(racers[2].chassis == "c");
    CHECK(racers[3].name == "SAM");
}

TEST_CASE("picking your own racer selects it, and a default racer joins your racers once") {
    settings::RacerStore store{.racers = {Named("ME", "a")}};
    const std::array defaults{Named("SAM", "d")};
    const std::vector<settings::Racer> racers = PickableRacers(store, defaults);

    SelectPicked(store, racers, 0);
    CHECK(store.selected == 0);
    CHECK(store.racers.size() == 1);

    SelectPicked(store, racers, 1);
    CHECK(store.selected == 1);
    REQUIRE(store.racers.size() == 2);
    CHECK(store.racers[1] == defaults[0]);

    SelectPicked(store, racers, 1);
    CHECK(store.selected == 1);
    CHECK(store.racers.size() == 2);

    SelectPicked(store, racers, 2);
    CHECK(store.selected == 1);
}

}
}
