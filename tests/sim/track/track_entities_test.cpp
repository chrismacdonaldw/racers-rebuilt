#include "sim/track/track_entities.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/archive_survey.hpp"
#include "sim/race/race_track.hpp"

namespace rr::sim {
namespace {

constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2.0F;

TrackEntitySources Arm() {
    return {
        .skeletons = {{.nodes = {{.name = "pivot"},
                                 {.name = "tip", .position = {.x = 10.0F}, .parent = 0}}}},
        .animations =
            {{.positionKeys = {},
              .rotationKeys = {Quaternion{}, FromAxisAngle({.z = 1.0F}, kQuarterTurn)},
              .keyFrames = {0, 10},
              .tracks = {{.rotations = {.firstValue = 0, .firstFrame = 0, .count = 2}}, {}},
              .parts =
                  {{.name = "swing", .frameCount = 21, .loopFrames = 22, .framesPerSecond = 100}}}},
        .entities = {{.name = "arm",
                      .startPart = 0,
                      .placement = {.position = {.x = 100.0F}},
                      .scale = 0.5F},
                     {.name = "still", .placement = {.position = {.y = 50.0F}}}},
    };
}

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

TEST_CASE("an entity's nodes chain up its skeleton, scaled, onto its placement") {
    const TrackEntitySources sources = Arm();
    TrackEntities entities(sources);

    const Vector3 before = entities.NodeWorld(0, 1).position;
    entities.Tick(Milliseconds{100});
    const Vector3 after = entities.NodeWorld(0, 1).position;

    CheckNear(before, {.x = 105.0F});
    CheckNear(after, {.x = 100.0F, .y = 5.0F});
    CHECK(entities.Clocks()[0] == PartClock{.frame = 10.0F});
}

TEST_CASE("an entity without a start part has no clock and stands in its rest pose") {
    const TrackEntitySources sources = Arm();
    TrackEntities entities(sources);

    entities.Tick(Milliseconds{100});

    CHECK_FALSE(entities.Clocks()[1].has_value());
    CheckNear(entities.NodeWorld(1, 1).position, {.x = 10.0F, .y = 50.0F});
}

TEST_CASE("a node the skeleton lacks stands at the entity's placement") {
    const TrackEntitySources sources = Arm();
    const TrackEntities entities(sources);

    CheckNear(entities.NodeWorld(0, 9).position, sources.entities[0].placement.position);
}

TEST_CASE("entities are found by name") {
    const TrackEntitySources sources = Arm();
    const TrackEntities entities(sources);

    CHECK(entities.Find("still") == 1);
    CHECK_FALSE(entities.Find("hammer").has_value());
}

TEST_CASE("a frozen entity holds its frame until it plays again") {
    const TrackEntitySources sources = Arm();
    TrackEntities entities(sources);
    entities.Tick(Milliseconds{50});

    entities.Freeze(0);
    entities.Tick(Milliseconds{100});
    CHECK(entities.Frame(0) == doctest::Approx(5.0F));
    entities.Play(0, 0, false);
    entities.Tick(Milliseconds{100});

    CHECK_FALSE(entities.IsFrozen(0));
    CHECK(entities.Clocks()[0] == PartClock{.frame = 10.0F, .loops = false});
}

TEST_CASE("playing out runs the current part on to its last frame and holds it") {
    const TrackEntitySources sources = Arm();
    TrackEntities entities(sources);
    entities.Freeze(0);
    entities.Tick(Milliseconds{100});

    entities.PlayOut(0);
    entities.Tick(Milliseconds{100});
    CHECK_FALSE(entities.IsHoldingLastFrame(0));
    entities.Tick(Milliseconds{500});

    CHECK(entities.IsHoldingLastFrame(0));
    CHECK(entities.Frame(0) == doctest::Approx(20.0F));
    CHECK(entities.IsHoldingLastFrame(1));
}

std::optional<RaceTrack> RetailTrack(std::string_view name) {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return std::nullopt;
    }
    Result<RaceTrack> track = LoadRaceTrack(*vfs, name);
    REQUIRE(track.has_value());
    return std::move(*track);
}

std::vector<std::string> EntityNames(const RaceTrack& track) {
    std::vector<std::string> names;
    std::ranges::transform(track.entities.entities, std::back_inserter(names), &TrackEntity::name);
    return names;
}

TEST_CASE("the RACEC0R0 world places its hammer and camera dummy") {
    const std::optional<RaceTrack> track = RetailTrack("RACEC0R0");
    if (!track) {
        return;
    }
    CHECK(EntityNames(*track) == std::vector<std::string>{"rkhamm02", "camDUM"});
}

TEST_CASE("the RACEC2R2 entities all start on their first part") {
    const std::optional<RaceTrack> track = RetailTrack("RACEC2R2");
    if (!track) {
        return;
    }
    CHECK(EntityNames(*track) ==
          std::vector<std::string>{"idol", "arktp", "dum02", "dum04", "dum", "dum03", "camDum"});
    CHECK(std::ranges::all_of(track->entities.entities,
                              [](const TrackEntity& entity) { return entity.startPart == 0; }));
}

struct HammerSwing {
    Vector3 pivot;
    Vector3 rest;
    Vector3 swung;
    Vector3 back;
};

HammerSwing SwingHammer(const RaceTrack& track) {
    TrackEntities entities(track.entities);
    const std::optional<std::size_t> found = entities.Find("rkhamm02");
    REQUIRE(found.has_value());
    const std::size_t hammer = *found;
    const auto run = [&](int ms) {
        for (int elapsed = 0; elapsed < ms; elapsed += 12) {
            entities.Tick(Milliseconds{12});
        }
        return entities.NodeWorld(hammer, 1).position;
    };
    HammerSwing swing{.pivot = entities.NodeWorld(hammer, 0).position,
                      .rest = entities.NodeWorld(hammer, 1).position};
    swing.swung = run(1683);
    swing.back = run(1684);
    return swing;
}

TEST_CASE("the RACEC0R0 hammer head swings about its pivot on a 101-frame loop") {
    const std::optional<RaceTrack> track = RetailTrack("RACEC0R0");
    if (!track) {
        return;
    }
    const HammerSwing swing = SwingHammer(*track);

    CHECK(Length(swing.rest - swing.pivot) == doctest::Approx(48.7F).epsilon(0.01));
    CHECK(Length(swing.swung - swing.pivot) == doctest::Approx(48.7F).epsilon(0.01));
    CHECK(Length(swing.swung - swing.rest) > 40.0F);
    CHECK(Length(swing.back - swing.rest) < 2.0F);
}

}
}
