#include "sim/hazard/lightning_cannon.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "hazard_rig.hpp"

namespace rr::sim {
namespace {

constexpr assets::EventId kTrigger = 9;
constexpr Vector3 kSource{.y = 500.0F, .z = 50.0F};

assets::Hazard Cannon(std::optional<std::string> entity = std::nullopt) {
    return {.kind = assets::HazardKind::LightningCannon,
            .payload = assets::LightningCannon{.trigger = kTrigger,
                                               .entity = std::move(entity),
                                               .source = kSource,
                                               .period = Milliseconds{2000},
                                               .sweepRadians = std::numbers::pi_v<float> / 2.0F,
                                               .verticalBase = -0.4F}};
}

const HazardAim& Aim(const test::HazardRig& rig) {
    return rig.Hazards().Aims()[0];
}

void TickFor(test::HazardRig& rig, int ticks) {
    for (int tick = 0; tick < ticks; ++tick) {
        rig.Tick();
    }
}

std::optional<LightningCannon> RetailCannon(const RaceTrack& track) {
    TrackEntities entities(track.entities);
    Random random(1);
    const Hazards hazards(track.hazards, entities, random);
    for (const HazardSlot& slot : hazards.Slots()) {
        if (const auto* cannon = std::get_if<LightningCannon>(&slot.state)) {
            return *cannon;
        }
    }
    return std::nullopt;
}

bool HasStarted(const test::HazardRig& rig) {
    return std::ranges::any_of(rig.Powerups().Events(), [](const PowerupEvent& event) {
        return event.kind == PowerupEventKind::Started;
    });
}

}

TEST_CASE("the lightning cannon fires an ownerless bolt on its first tick") {
    test::HazardRig rig({Cannon()}, {});
    rig.Hear(kTrigger, EventEdge::Start);

    rig.Tick();

    REQUIRE(rig.Powerups().Active().size() == 1);
    CHECK(rig.Effect().kind == PowerupKind::Lightning);
    CHECK(rig.Effect().use.hazard == 0);
    CHECK(Aim(rig).source == kSource);
}

TEST_CASE("the lightning cannon sweeps out and back over its period, dipping by its base") {
    test::HazardRig rig({Cannon()}, {});
    rig.Hear(kTrigger, EventEdge::Start);

    TickFor(rig, 83);
    CHECK(Aim(rig).direction.y > 0.92F);
    CHECK(Aim(rig).direction.z == doctest::Approx(-0.4F / std::sqrt(1.16F)));

    TickFor(rig, 42);
    CHECK(Aim(rig).direction.x == doctest::Approx(Aim(rig).direction.y));

    TickFor(rig, 42);
    CHECK(Aim(rig).direction.y == doctest::Approx(0.0F).epsilon(0.001));
}

TEST_CASE("the lightning cannon fires again after 8.3 s running") {
    test::HazardRig rig({Cannon()}, {});
    rig.Hear(kTrigger, EventEdge::Start);
    rig.Tick();

    TickFor(rig, 691);
    CHECK_FALSE(HasStarted(rig));
    rig.Tick();

    CHECK(HasStarted(rig));
}

TEST_CASE("the lightning cannon aims from 17 below its entity's root") {
    test::HazardRig rig({Cannon("arktp")},
                        test::StillEntities({"arktp"}, {.x = 10.0F, .y = 20.0F, .z = 30.0F}));
    rig.Hear(kTrigger, EventEdge::Start);

    rig.Tick();

    CHECK(Aim(rig).source == Vector3{.x = 10.0F, .y = 20.0F, .z = 13.0F});
}

TEST_CASE("the retail sweep cannons aim from their payload and their tower") {
    const std::optional<RaceTrack> moon = test::RetailRaceTrack("RACEC2R3");
    const std::optional<RaceTrack> tomb = test::RetailRaceTrack("RACEC2R2");
    if (!moon || !tomb) {
        return;
    }
    const Vector3 source = RetailCannon(*moon).value_or(LightningCannon{}).source;
    CHECK(source.x == doctest::Approx(534.041F));
    CHECK(source.y == doctest::Approx(370.596F));
    CHECK(source.z == doctest::Approx(46.0F));
    CHECK(RetailCannon(*tomb).value_or(LightningCannon{}).entity.has_value());
}

TEST_CASE("on a mirrored track the lightning cannon sweeps the other way") {
    test::HazardRig rig({Cannon()}, {});
    rig.MirrorTrack();
    rig.Hear(kTrigger, EventEdge::Start);

    TickFor(rig, 83);

    CHECK(Aim(rig).direction.y < -0.92F);
}

}
