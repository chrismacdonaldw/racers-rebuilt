#include "sim/hazard/code_puzzle.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <optional>
#include <variant>
#include <vector>

#include "hazard_rig.hpp"
#include "sim/collision/world_list.hpp"
#include "sim/race/race_session.hpp"
#include "sim/race/race_track.hpp"

namespace rr::sim {
namespace {

assets::Hazard PuzzleHazard() {
    return {.kind = assets::HazardKind::CodePuzzle};
}

const CodePuzzle& Puzzle(const test::HazardRig& rig) {
    return std::get<CodePuzzle>(rig.Hazards().Slots()[0].state);
}

assets::EventId WrongGate(assets::EventId gate) {
    return gate % 2 == 0 ? gate + 1 : gate - 1;
}

const CodePuzzle* RetailPuzzle(const RaceSession& session) {
    for (const HazardSlot& slot : session.TrackHazards().Slots()) {
        if (const auto* puzzle = std::get_if<CodePuzzle>(&slot.state)) {
            return puzzle;
        }
    }
    return nullptr;
}

void CrossRetailGate(const RaceTrack& track, RaceSession& session, assets::EventId gate) {
    const auto sphere =
        std::ranges::find(track.events.racerSpheres, gate, &assets::TriggerSphere::event);
    REQUIRE(sphere != track.events.racerSpheres.end());
    const std::vector<RacerControls> idle(1);
    session.Racers()[0].Place({.position = sphere->centre});
    session.Tick(Milliseconds{12}, idle);
    session.Racers()[0].Place({.position = {.x = 1000.0F, .y = 1000.0F}});
    session.Tick(Milliseconds{12}, idle);
}

void TickTimes(test::HazardRig& rig, int ticks) {
    for (int tick = 0; tick < ticks; ++tick) {
        rig.Tick();
    }
}

void Cross(test::HazardRig& rig, assets::EventId gate) {
    rig.Tick();
    rig.Hear({.id = gate, .racer = 0});
    rig.Hear({.id = gate, .position = Vector3{}});
}

}

TEST_CASE("the code puzzle runs from load with one gate of each pair to take") {
    const test::HazardRig rig({PuzzleHazard()}, {});

    CHECK(rig.Hazards().Slots()[0].phase == HazardPhase::Active);
    for (std::size_t gate = 0; gate < kCodeGates; ++gate) {
        const auto red = static_cast<assets::EventId>(200 + (gate * 2));
        CHECK((Puzzle(rig).code[gate] == red || Puzzle(rig).code[gate] == red + 1));
    }
}

TEST_CASE("taking the three gates of the code opens the doors and draws a new code") {
    test::HazardRig rig({PuzzleHazard()}, {});
    const auto code = Puzzle(rig).code;

    Cross(rig, code[0]);
    Cross(rig, code[1]);
    CHECK(Puzzle(rig).progress == 3);
    Cross(rig, code[2]);

    CHECK(rig.CountStarts(kCodeSolvedEvent) == 1);
    CHECK(rig.CountStarts(kCodeJingleEvent) == 1);
    CHECK(Puzzle(rig).progress == 0);
    CHECK(Puzzle(rig).revealLeft == Milliseconds{2'500});
}

TEST_CASE("the puzzle hears gates from the queue it fires into") {
    const TrackEntitySources sources;
    TrackEntities entities(sources);
    Random random(1);
    const std::vector<assets::Hazard> list{PuzzleHazard()};
    Hazards hazards(list, entities, random);
    TrackEvents events({}, 1, false, random, Hazards::MostEventsPerTick(list, 1));
    HazardContext context{
        .entities = &entities, .events = &events, .models = &hazards.Models(), .random = &random};
    for (const assets::EventId gate : std::get<CodePuzzle>(hazards.Slots()[0].state).code) {
        events.Fire({.id = gate, .position = Vector3{}});
    }

    hazards.Hear(events, context);

    CHECK(std::ranges::count(events.Events(), kCodeSolvedEvent, &TrackEvent::id) == 2);
    CHECK(std::get<CodePuzzle>(hazards.Slots()[0].state).revealLeft > Milliseconds{0});
}

TEST_CASE("a wrong gate starts the code over") {
    test::HazardRig rig({PuzzleHazard()}, {});
    const auto code = Puzzle(rig).code;

    Cross(rig, code[0]);
    Cross(rig, WrongGate(code[1]));
    Cross(rig, code[1]);
    Cross(rig, code[2]);

    CHECK(rig.CountStarts(kCodeSolvedEvent) == 0);
    CHECK(Puzzle(rig).progress == 0);
}

TEST_CASE("gates sound red or blue at the racer") {
    test::HazardRig rig({PuzzleHazard()}, {});
    rig.Place(0, {.x = 5.0F, .z = 0.2F});

    rig.Tick();
    rig.Hear({.id = 200, .racer = 0});
    REQUIRE(rig.Events().size() == 2);
    CHECK(rig.Events()[0].id == 21);
    CHECK(rig.Events()[0].position.value_or(Vector3{}).x == doctest::Approx(5.0F));

    rig.Tick();
    rig.Hear({.id = 203, .position = Vector3{}});
    CHECK(rig.CountStarts(30) == 1);
}

TEST_CASE("the reveal shows the colour of each gate of the code") {
    test::HazardRig rig({PuzzleHazard()}, {});

    rig.Tick();
    rig.Hear({.id = 208, .position = Vector3{}});
    CHECK(rig.CountStarts(Puzzle(rig).code[1] % 2 == 0 ? 29 : 20) == 1);
}

TEST_CASE("the jingle ends and the new code shows 2.5 s after the doors open") {
    test::HazardRig rig({PuzzleHazard()}, {});
    const auto code = Puzzle(rig).code;
    Cross(rig, code[0]);
    Cross(rig, code[1]);
    Cross(rig, code[2]);

    for (int tick = 0; tick < 2'500 / 12; ++tick) {
        rig.Tick();
    }
    CHECK(Puzzle(rig).revealLeft > Milliseconds{0});
    rig.Tick();

    CHECK(Puzzle(rig).revealLeft == Milliseconds{0});
    CHECK(rig.Events().back() == TrackEvent{.id = kCodeJingleEvent, .edge = EventEdge::End});
}

TEST_CASE("the lamps hold the code from load") {
    const test::HazardRig rig({PuzzleHazard()}, {});

    CHECK(Puzzle(rig).IsLampRed(1) == (Puzzle(rig).code[1] % 2 == 0));
    CHECK_FALSE(Puzzle(rig).Flashing());
}

TEST_CASE("the lamps hold the old code while they flash, then the new one") {
    test::HazardRig rig({PuzzleHazard()}, {});
    const auto code = Puzzle(rig).code;
    Cross(rig, code[0]);
    Cross(rig, code[1]);
    Cross(rig, code[2]);

    rig.Tick();
    CHECK(Puzzle(rig).Flashing() == Milliseconds{12});
    CHECK(Puzzle(rig).lampCode == code);
    TickTimes(rig, 2'500 / 12);

    CHECK_FALSE(Puzzle(rig).Flashing());
    CHECK(Puzzle(rig).lampCode == Puzzle(rig).code);
}

TEST_CASE("end edges and other ids leave the code alone") {
    test::HazardRig rig({PuzzleHazard()}, {});
    const auto code = Puzzle(rig).code;

    Cross(rig, code[0]);
    rig.Hear({.id = WrongGate(code[1]), .edge = EventEdge::End});
    rig.Hear({.id = 10});

    CHECK(Puzzle(rig).progress == 1);
}

TEST_CASE("taking the retail code gates in order opens the doors") {
    const std::optional<RaceTrack> track = test::RetailRaceTrack("RACEC0R3");
    if (!track) {
        return;
    }
    const WorldList world(track->collision.Track());
    Result<RaceSession> session = test::PlayerSession(*track, world);
    REQUIRE(session.has_value());
    const CodePuzzle* puzzle = RetailPuzzle(*session);
    REQUIRE(puzzle != nullptr);
    const std::size_t door = session->Entities().Find("rtdor1").value_or(0);
    const std::optional<std::size_t> closed = session->Entities().CurrentPart(door);
    const auto code = puzzle->code;
    const std::vector<RacerControls> idle(1);

    for (const assets::EventId gate : code) {
        CrossRetailGate(*track, *session, gate);
    }
    for (int tick = 0; tick < 10; ++tick) {
        session->Tick(Milliseconds{12}, idle);
    }

    CHECK(puzzle->revealLeft > Milliseconds{0});
    CHECK(session->Entities().CurrentPart(door) != closed);
}

}
