#include "app/trigger_spheres.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <vector>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kTick{12};

sim::TrackEventSources Sources() {
    return {.racerSpheres = {{.centre = {.x = 10.0F}, .radius = 5.0F, .event = 1}},
            .environmentSpheres = {{.centre = {.y = 40.0F}, .radius = 8.0F, .event = 2}}};
}

TEST_CASE("each trigger sphere is drawn as three rings at its radius") {
    sim::Random random(1);
    const sim::TrackEvents triggers(Sources(), 1, false, random);
    std::vector<render::DebugLine> lines;

    DrawTriggerSpheres(triggers, lines);

    REQUIRE(lines.size() == 144);
    const Vector3 centre = ToEngine({.x = 10.0F});
    CHECK(Length(lines.front().from - centre) == doctest::Approx(5.0F));
    CHECK(Length(lines.back().to - ToEngine({.y = 40.0F})) == doctest::Approx(8.0F));
}

TEST_CASE("a trigger sphere lights up while a racer is inside") {
    sim::Random random(1);
    sim::TrackEvents triggers(Sources(), 1, false, random);
    std::vector<render::DebugLine> idle;
    DrawTriggerSpheres(triggers, idle);
    const std::array racers{
        sim::TriggerRacer{.reach = {.centre = {.x = 10.0F}, .radius = 1.0F}, .isPlayer = true}};
    triggers.BeginTick();
    triggers.Tick(kTick, racers, random);
    std::vector<render::DebugLine> inside;

    DrawTriggerSpheres(triggers, inside);

    REQUIRE(inside.size() == idle.size());
    CHECK(inside.front().color.g != idle.front().color.g);
    CHECK(inside.back().color.g == idle.back().color.g);
}

}
}
