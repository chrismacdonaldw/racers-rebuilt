#include "app/racer/racer_ref.hpp"

#include <doctest/doctest.h>

#include <filesystem>

#include "app/racer/racer_lookup.hpp"
#include "assets/vfs.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

TEST_CASE("a racer ref tells copies with the same name apart by their hash") {
    const settings::Racer first{.name = "Copy", .chassis = "a"};
    const settings::Racer second{.name = "COPY", .chassis = "b"};

    CHECK(IsRef(first, {.name = "COPY"}));
    CHECK(IsRef(second, {.name = "copy"}));
    CHECK(IsRef(second, RefOf(second)));
    CHECK_FALSE(IsRef(first, RefOf(second)));
    CHECK(RacerHash(first) != RacerHash(second));
}

TEST_CASE("finding a racer by ref picks the copy its hash names") {
    const rr::test::TempFolder folder;
    const std::filesystem::path file = folder.Path() / "racers.toml";
    const settings::Racer first{.name = "COPY", .chassis = "a"};
    const settings::Racer second{.name = "COPY", .chassis = "b"};
    REQUIRE(settings::SaveRacers({.racers = {first, second}}, file).has_value());

    const Result<settings::Racer> byName = FindRacer(assets::Vfs{}, file, {.name = "COPY"});
    const Result<settings::Racer> byRef = FindRacer(assets::Vfs{}, file, RefOf(second));

    REQUIRE(byName.has_value());
    REQUIRE(byRef.has_value());
    CHECK(byName->chassis == "a");
    CHECK(byRef->chassis == "b");
}

}
}
