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
    Hazards hazards(list, entities);
    const std::vector<TrackEvent> events{{.id = kHammerSwishTrigger, .racer = 0},
                                         {.id = kHammerSwishTrigger + 1}};

    hazards.Hear(events);

    CHECK(hazards.Slots()[0].phase == HazardPhase::Loaded);
}

TEST_CASE("kinds the race doesn't run yet never start") {
    test::HazardRig rig({{.kind = assets::HazardKind::FallingPillar}}, {});

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
    const RaceSetup setup{.racers = {{.isPlayer = true}}};
    Result<RaceSession> session = RaceSession::Create(setup, {.checkpoints = &track->checkpoints,
                                                              .grid = track->grid,
                                                              .world = &world,
                                                              .lowestTrackZ = track->lowestTrackZ,
                                                              .events = &track->events,
                                                              .entities = &track->entities,
                                                              .hazards = track->hazards});
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

std::size_t CountRunning(const assets::Vfs& vfs, const std::string& name) {
    CAPTURE(name);
    Result<RaceTrack> track = LoadRaceTrack(vfs, name);
    REQUIRE(track.has_value());
    TrackEntities entities(track->entities);
    const Hazards hazards(track->hazards, entities);
    std::size_t running = 0;
    for (const HazardSlot& slot : hazards.Slots()) {
        std::visit(
            [&](const auto& hazard) {
                if constexpr (requires { hazard.entity; }) {
                    CHECK(hazard.entity.has_value());
                    ++running;
                }
            },
            slot.state);
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
    CHECK(running == 6);
}

}
}
