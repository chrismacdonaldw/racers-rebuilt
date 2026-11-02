#include "app/circuit_leg.hpp"

#include <doctest/doctest.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "app/menu/award_scene_name.hpp"
#include "app/menu/menu_scenes.hpp"

namespace rr::app {
namespace {

using Places = std::array<std::optional<int>, assets::kGridSlots>;

constexpr Places kPlayerWins{1, 2, 3, 4, 5, 6};
constexpr Places kPlayerLast{6, 1, 2, 3, 4, 5};

assets::RaceEntry Race(std::string folder, std::string circuit, std::int32_t position) {
    return {.name = folder,
            .folder = std::move(folder),
            .circuit = std::move(circuit),
            .position = position};
}

assets::RaceCatalog Catalog() {
    Result<assets::RaceCatalog> catalog = assets::RaceCatalog::Create(
        {{.name = "c0"}, {.name = "c6"}, {.name = "c9"}},
        {Race("racec0r1", "c0", 0), Race("racec1r0", "c0", 1), Race("racec0r3", "c0", 2),
         Race("racec0r2", "c0", 3), Race("racec3r0", "c6", 0)});
    REQUIRE(catalog.has_value());
    return std::move(*catalog);
}

CircuitSceneSetup Start(std::string circuit) {
    return {.race = {.circuit = std::move(circuit), .respawn = {.isEnabled = false}, .racer = 2}};
}

CircuitLeg Leg(const assets::RaceCatalog& catalog, std::string_view scene) {
    Result<CircuitLeg> leg = ParseCircuitScene(scene).and_then(
        [&](const CircuitSceneSetup& setup) { return CircuitLeg::Create(catalog, setup); });
    REQUIRE_MESSAGE(leg.has_value(), scene);
    return std::move(*leg);
}

TEST_CASE("a circuit plays its four races from scripted finishes to its award") {
    const assets::RaceCatalog catalog = Catalog();
    std::string scene = CircuitSceneName(Start("c0"));
    std::vector<std::string> tracks;

    while (IsCircuitScene(scene)) {
        const CircuitLeg leg = Leg(catalog, scene);
        tracks.push_back(leg.Race().track);
        CHECK(leg.Race().racer == 2);
        scene = leg.AfterFinish(kPlayerWins);
    }

    CHECK(tracks == std::vector<std::string>{"RACEC0R1", "RACEC1R0", "RACEC0R3", "RACEC0R2"});
    CHECK(scene == AwardSceneName(sim::CircuitAward::Gold));
}

TEST_CASE("each race starts with the points the races before it scored") {
    const assets::RaceCatalog catalog = Catalog();
    const CircuitLeg first = Leg(catalog, CircuitSceneName(Start("c0")));

    const CircuitLeg second = Leg(catalog, first.AfterFinish(kPlayerWins));

    CHECK(first.Points()[0] == 0);
    CHECK(second.Points()[0] == 30);
    CHECK(second.Points()[5] == 1);
}

TEST_CASE("a player without enough points is knocked out to the losing award") {
    const assets::RaceCatalog catalog = Catalog();
    const CircuitLeg first = Leg(catalog, CircuitSceneName(Start("c0")));

    CHECK(first.AfterFinish(kPlayerLast) == AwardSceneName(sim::CircuitAward::None));
}

TEST_CASE("restart goes back to the first race with no points, and quit leaves the circuit") {
    const assets::RaceCatalog catalog = Catalog();
    const CircuitLeg second =
        Leg(catalog, Leg(catalog, CircuitSceneName(Start("c0"))).AfterFinish(kPlayerWins));

    CHECK(second.AfterRestart() == CircuitSceneName(Start("c0")));
    CHECK(second.AfterQuit() == kMainMenuScene);
}

TEST_CASE("a one-race circuit awards after its only race") {
    const assets::RaceCatalog catalog = Catalog();
    const CircuitLeg leg = Leg(catalog, CircuitSceneName(Start("c6")));

    CHECK(leg.Race().track == "RACEC3R0");
    CHECK(leg.AfterFinish(kPlayerWins) == AwardSceneName(sim::CircuitAward::Gold));
}

TEST_CASE("a circuit that's missing, empty or already over doesn't start") {
    const assets::RaceCatalog catalog = Catalog();
    CircuitSceneSetup over = Start("c6");
    over.standing.race = 1;

    CHECK_FALSE(CircuitLeg::Create(catalog, Start("c5")).has_value());
    CHECK_FALSE(CircuitLeg::Create(catalog, Start("c9")).has_value());
    CHECK_FALSE(CircuitLeg::Create(catalog, over).has_value());
}

}
}
