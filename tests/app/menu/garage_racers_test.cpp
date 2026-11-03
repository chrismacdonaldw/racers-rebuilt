#include "app/menu/garage_racers.hpp"

#include <doctest/doctest.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "assets/archive_survey.hpp"

namespace rr::app {
namespace {

settings::Racer MadeRacer(const assets::Vfs& vfs) {
    Result<settings::Racer> made = NewRacer(vfs, "NEW RACER");
    REQUIRE_MESSAGE(made.has_value(), made.error().message);
    return std::move(*made);
}

settings::Racer Named(std::string name) {
    return {.name = std::move(name), .chassis = "CHASS1"};
}

TEST_CASE("the garage copies a racer whole, keeping its name") {
    settings::RacerStore store{.racers = {Named("JOAN"), Named("BOB")}, .selected = 1};
    store.racers.front().trophies["c0"] = settings::Trophy::Gold;

    CHECK(CopyRacer(store, 0) == 2);
    CHECK(store.racers.at(2) == store.racers.at(0));
    CHECK(store.selected == 1);
}

TEST_CASE("deleting a racer keeps the selection on the same racer") {
    settings::RacerStore store{.racers = {Named("A"), Named("B"), Named("C")}, .selected = 2};

    DeleteRacer(store, 0);
    CHECK(store.racers.size() == 2);
    CHECK(store.racers.front().name == "B");
    CHECK(store.selected == 1);
    DeleteRacer(store, 0);
    CHECK(store.selected == 0);
    DeleteRacer(store, 0);
    CHECK(store.racers.empty());
    CHECK_FALSE(store.selected.has_value());
}

TEST_CASE("deleting a racer after the selected one leaves the selection alone") {
    settings::RacerStore store{.racers = {Named("A"), Named("B")}, .selected = 0};

    DeleteRacer(store, 1);
    CHECK(store.selected == 0);
}

TEST_CASE("a new racer, copied and deleted, drives the store the garage saves") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const settings::Racer made = MadeRacer(*vfs);
    CHECK(made.chassis == "ddchas0");
    CHECK(made.car == assets::CarData{});

    settings::RacerStore store{.racers = {made}};
    CHECK(CopyRacer(store, 0) == 1);
    DeleteRacer(store, 0);
    CHECK(store.racers == std::vector{made});
}

}
}
