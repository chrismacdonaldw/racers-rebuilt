#include "app/powerup/powerup_visuals.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr float kFrameMs = 16.0F;
constexpr float kModelScale = 1.0F / 64.0F;
constexpr Vector3 kAt{.x = 10.0F, .y = 5.0F};

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

Vector3 GamePosition(const render::MeshDraw& draw) {
    return TransformToGame(draw.transform).position;
}

assets::AnimationPart Part(std::string name, std::uint32_t track) {
    return {.name = std::move(name),
            .firstTrack = track,
            .frameCount = 10,
            .loopFrames = 11,
            .framesPerSecond = 30,
            .velocity = {},
            .bounds = {}};
}

JointedAssets OneNodeShifts(const std::vector<std::string>& parts) {
    JointedAssets jointed{
        .skeleton = {.nodes = {{.name = "root", .position = {}, .rotation = {}, .parent = {}}}},
        .animations = {}};
    const std::vector<Vector3> shifts{{.x = 64.0F}, {.y = 64.0F}, {.z = 64.0F}};
    for (std::uint32_t i = 0; i < parts.size(); ++i) {
        jointed.animations.positionKeys.push_back(shifts.at(i));
        jointed.animations.tracks.push_back(
            {.rotations = {}, .positions = {.firstValue = i, .count = 1}});
        jointed.animations.parts.push_back(Part(parts[i], i));
    }
    return jointed;
}

EffectModel Static() {
    return {.draws = {.draws = std::vector<render::MeshDraw>(1), .joints = {0}, .scale = 1.0F},
            .jointed = nullptr};
}

EffectModel Jointed(const JointedAssets& jointed) {
    return {
        .draws = {.draws = std::vector<render::MeshDraw>(1), .joints = {0}, .scale = kModelScale},
        .jointed = &jointed};
}

sim::RenderSnapshot WithEffect(sim::PowerupKind kind, sim::PowerupUse use, std::uint8_t phase = 0) {
    sim::RenderSnapshot snapshot;
    snapshot.effects.push_back({.slot = 3,
                                .kind = kind,
                                .use = use,
                                .phase = phase,
                                .transform = TransformToEngine({.position = kAt})});
    return snapshot;
}

TEST_CASE("a shield draws its outer and inner shells at its pose") {
    const EffectModels models{{"SHIELD1", Static()}, {"SHLDIN1", Static()}, {"SHIELD2", Static()}};
    PowerupVisuals visuals(models);

    visuals.Update(WithEffect(sim::PowerupKind::Shield, {.level = 1}), kFrameMs);

    REQUIRE(visuals.Draws().size() == 2);
    CheckNear(GamePosition(visuals.Draws()[0]), kAt);
    CheckNear(GamePosition(visuals.Draws()[1]), kAt);
}

TEST_CASE("a turbo's flames fire up, sustain and die out with its phases") {
    const JointedAssets flames = OneNodeShifts({"fireup", "sustain", "dieout"});
    const EffectModels models{{"TURBOL0", Jointed(flames)}};
    PowerupVisuals visuals(models);

    visuals.Update(WithEffect(sim::PowerupKind::Turbo, {}, 0), kFrameMs);
    CheckNear(GamePosition(visuals.Draws()[0]), kAt + Vector3{.x = 1.0F});
    visuals.Update(WithEffect(sim::PowerupKind::Turbo, {}, 1), kFrameMs);
    CheckNear(GamePosition(visuals.Draws()[0]), kAt + Vector3{.y = 1.0F});
    visuals.Update(WithEffect(sim::PowerupKind::Turbo, {}, 2), kFrameMs);
    CheckNear(GamePosition(visuals.Draws()[0]), kAt + Vector3{.z = 1.0F});
}

TEST_CASE("a missile plays the launch for its shot") {
    const JointedAssets missile = OneNodeShifts({"right", "middle", "left"});
    const EffectModels models{{"DMISSIL", Jointed(missile)}};
    PowerupVisuals visuals(models);

    visuals.Update(WithEffect(sim::PowerupKind::Missile, {.shot = 2}), kFrameMs);

    CheckNear(GamePosition(visuals.Draws()[0]), kAt + Vector3{.z = 1.0F});
}

TEST_CASE("a warp portal shows only while the warp opens") {
    const EffectModels models{{"WARPPRT", Static()}};
    PowerupVisuals visuals(models);

    visuals.Update(WithEffect(sim::PowerupKind::Warp, {.level = 3}, 0), kFrameMs);
    CHECK(visuals.Draws().size() == 1);
    visuals.Update(WithEffect(sim::PowerupKind::Warp, {.level = 3}, 1), kFrameMs);
    CHECK(visuals.Draws().empty());
}

TEST_CASE("explosions draw their own model, scaled by the blast's growth") {
    const EffectModels models{{"EXPLSN", Static()}, {"SPIKEXP", Static()}};
    PowerupVisuals visuals(models);
    sim::RenderSnapshot snapshot;
    snapshot.explosions = {{.kind = sim::ExplosionKind::Blast, .transform = {}, .growth = 0.5F},
                           {.kind = sim::ExplosionKind::Spike, .transform = {}, .growth = 0.25F},
                           {.kind = sim::ExplosionKind::Blast, .transform = {}, .growth = 0.0F}};

    visuals.Update(snapshot, kFrameMs);

    REQUIRE(visuals.Draws().size() == 2);
    CHECK(visuals.Draws()[0].scale == doctest::Approx(0.5F));
    CHECK(visuals.Draws()[1].scale == doctest::Approx(0.25F));
}

TEST_CASE("power-ups without models, and effects that ended, draw nothing") {
    const EffectModels models{{"CURSE", Static()}};
    PowerupVisuals visuals(models);

    visuals.Update(WithEffect(sim::PowerupKind::Cannonball, {}), kFrameMs);
    CHECK(visuals.Draws().empty());
    visuals.Update(WithEffect(sim::PowerupKind::Curse, {.colour = assets::BrickColour::Yellow}),
                   kFrameMs);
    CHECK(visuals.Draws().size() == 1);
    visuals.Update({}, kFrameMs);
    CHECK(visuals.Draws().empty());
}

}
}
