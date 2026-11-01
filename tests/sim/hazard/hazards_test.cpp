#include "sim/hazard/hazards.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/track_loader.hpp"
#include "hazard_rig.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/race_track.hpp"

namespace rr::sim {
namespace {

using enum EventEdge;

HazardPhase PhaseOf(const test::HazardRig& rig, std::size_t slot = 0) {
    return rig.Hazards().Slots()[slot].phase;
}

TEST_CASE("a hazard runs from its trigger's start to its end on the position path") {
    test::HazardRig rig({{.kind = assets::HazardKind::HammerSwish}},
                        test::StillEntities({"rkhamm02"}, {}));

    rig.Hear(kHammerSwishTrigger, Start);
    CHECK(PhaseOf(rig) == HazardPhase::Active);
    rig.Hear(kHammerSwishTrigger, End);
    CHECK(PhaseOf(rig) == HazardPhase::Loaded);
}

TEST_CASE("racer-path events and other ids leave a hazard alone") {
    const TrackEntitySources sources = test::StillEntities({"rkhamm02"}, {});
    TrackEntities entities(sources);
    const std::vector<assets::Hazard> list{{.kind = assets::HazardKind::HammerSwish}};
    Random random(1);
    Hazards hazards(list, entities, random);
    TrackEvents heard({}, 1, false, random, 2);
    heard.Fire({.id = kHammerSwishTrigger, .racer = 0});
    heard.Fire({.id = kHammerSwishTrigger + 1});
    HazardContext context;

    hazards.Hear(heard, context);

    CHECK(hazards.Slots()[0].phase == HazardPhase::Loaded);
}

TEST_CASE("kinds the race doesn't run yet never start") {
    test::HazardRig rig({{.kind = assets::HazardKind::WaterScroll}}, {});

    rig.Hear(10, Start);

    CHECK(PhaseOf(rig) == HazardPhase::Loaded);
    CHECK(std::holds_alternative<std::monostate>(rig.Hazards().Slots()[0].state));
}

TEST_CASE("a racer in the hammer's sphere starts the hammer, and leaving stops it") {
    const std::optional<RaceTrack> track = test::RetailRaceTrack("RACEC0R0");
    if (!track) {
        return;
    }
    const WorldList world(track->collision.Track());
    Result<RaceSession> session = test::PlayerSession(*track, world);
    REQUIRE(session.has_value());
    const auto hammer = std::ranges::find_if(session->TrackHazards().Slots(), [](const auto& slot) {
        return std::holds_alternative<HammerSwish>(slot.state);
    });
    REQUIRE(hammer != session->TrackHazards().Slots().end());
    const std::vector<RacerControls> idle(1);

    session->Racers()[0].Place({.position = {.x = 11.141F, .y = -621.298F, .z = -13.602F}});
    session->Tick(Milliseconds{12}, idle);
    CHECK(hammer->phase == HazardPhase::Active);
    session->Racers()[0].Place({.position = {.x = 1000.0F, .y = 1000.0F}});
    session->Tick(Milliseconds{12}, idle);
    session->Tick(Milliseconds{12}, idle);
    CHECK(hammer->phase == HazardPhase::Loaded);
}

std::optional<bool> FoundItsModel(const HazardState& state, const assets::Hazard& spec) {
    const auto* cannon = std::get_if<assets::LightningCannon>(&spec.payload);
    if (cannon != nullptr && !cannon->entity) {
        return std::nullopt;
    }
    return std::visit(
        [](const auto& hazard) -> std::optional<bool> {
            if constexpr (requires { hazard.entity.has_value(); }) {
                return hazard.entity.has_value();
            } else if constexpr (requires { hazard.reach.has_value(); }) {
                return hazard.reach.has_value();
            } else if constexpr (requires { hazard.entities.empty(); }) {
                return !hazard.entities.empty();
            } else if constexpr (requires { hazard.targets.empty(); }) {
                return !hazard.targets.empty();
            } else {
                return std::nullopt;
            }
        },
        state);
}

std::size_t CountRunning(const assets::Vfs& vfs, const std::string& name) {
    CAPTURE(name);
    Result<RaceTrack> track = LoadRaceTrack(vfs, name);
    REQUIRE(track.has_value());
    TrackEntities entities(track->entities);
    Random random(1);
    const Hazards hazards(track->hazards, entities, random);
    std::size_t running = 0;
    for (std::size_t i = 0; i < hazards.Slots().size(); ++i) {
        if (const std::optional<bool> found =
                FoundItsModel(hazards.Slots()[i].state, track->hazards[i])) {
            CHECK(*found);
            ++running;
        }
    }
    return running;
}

TEST_CASE("every retail hazard this race runs finds its entity") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    std::size_t running = 0;
    for (const std::string& name : assets::ListRaceTracks(*vfs)) {
        running += CountRunning(*vfs, name);
    }
    CHECK(running == 20);
}

}
TEST_CASE("retail hazards size their spheres from their models") {
    const std::optional<RaceTrack> ghostTrack = test::RetailRaceTrack("RACEC1R0");
    const std::optional<RaceTrack> lavaTrack = test::RetailRaceTrack("RACEC0R3");
    if (!ghostTrack || !lavaTrack) {
        return;
    }
    const auto radiusOf = [](const RaceTrack& track, std::string_view name) {
        return std::ranges::find(track.entities.entities, name, &TrackEntity::name)->radius;
    };
    CHECK(radiusOf(*ghostTrack, "ghostly") == doctest::Approx(4.747F).epsilon(0.001));
    CHECK(radiusOf(*lavaTrack, "mmlavbl") == doctest::Approx(5.259F).epsilon(0.001));

    const TrackEntities entities(lavaTrack->entities);
    const std::optional<Transform> pad = entities.FindStatic("mmrocof");
    CHECK(pad.value_or(Transform{}).position.x == doctest::Approx(411.055F));
}

}
