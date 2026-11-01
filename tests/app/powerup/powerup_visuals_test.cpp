#include "app/powerup/powerup_visuals.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "app/track_space.hpp"
#include "sim/powerup/magnet_effect.hpp"

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

JointedAssets SlidesNineAlongX() {
    JointedAssets jointed{
        .skeleton = {.nodes = {{.name = "root", .position = {}, .rotation = {}, .parent = {}}}},
        .animations = {}};
    jointed.animations.positionKeys = {{}, {.x = 9.0F * 64.0F}};
    jointed.animations.keyFrames = {0, 9};
    jointed.animations.tracks.push_back(
        {.rotations = {}, .positions = {.firstValue = 0, .firstFrame = 0, .count = 2}});
    jointed.animations.parts.push_back(Part("slide", 0));
    return jointed;
}

EffectModel Static() {
    return {.draws = {.draws = std::vector<render::MeshDraw>(1), .joints = {0}, .scale = 1.0F},
            .jointed = nullptr};
}

EffectModel Scrolling(assets::TextureScroll scroll) {
    EffectModel model = Static();
    model.scroll = scroll;
    return model;
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

TEST_CASE("a curse floats over its glow, which lies on the ground") {
    const EffectModels models{{"CURSE", Static()}, {"CGREEN", Static()}, {"CGREEN2", Static()}};
    PowerupVisuals visuals(models);

    visuals.Update(WithEffect(sim::PowerupKind::Curse, {.level = 3}), kFrameMs);

    REQUIRE(visuals.Draws().size() == 3);
    CheckNear(GamePosition(visuals.Draws()[0]), kAt);
    CheckNear(GamePosition(visuals.Draws()[1]), kAt - Vector3{.z = 13.0F});
    CheckNear(GamePosition(visuals.Draws()[2]), kAt - Vector3{.z = 13.0F});
}

TEST_CASE("a power-up model's animation runs on from frame to frame") {
    const JointedAssets slide = SlidesNineAlongX();
    const EffectModels models{{"SHIELD0", Jointed(slide)}};
    PowerupVisuals visuals(models);

    visuals.Update(WithEffect(sim::PowerupKind::Shield, {}), 100.0F);
    visuals.Update(WithEffect(sim::PowerupKind::Shield, {}), 100.0F);

    CheckNear(GamePosition(visuals.Draws()[0]), kAt + Vector3{.x = 6.0F});
}

TEST_CASE("a shield's texture scrolls from when it's raised") {
    const EffectModels models{{"SHIELD0", Scrolling({.vPerSecond = 4.0F})},
                              {"SHLDIN0", Scrolling({.uPerSecond = -0.5F})}};
    PowerupVisuals visuals(models);

    visuals.Update(WithEffect(sim::PowerupKind::Shield, {}), 125.0F);
    visuals.Update(WithEffect(sim::PowerupKind::Shield, {}), 250.0F);

    CHECK(visuals.Draws()[0].material.uvOffset.v == doctest::Approx(0.5F));
    CHECK(visuals.Draws()[1].material.uvOffset.u == doctest::Approx(0.8125F));
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

TEST_CASE("a ghost racer's turbo flames are hidden") {
    const JointedAssets jointed = OneNodeShifts({"fireup", "sustain", "dieout"});
    const EffectModels models{{"TURBOL0", Jointed(jointed)}};
    PowerupVisuals visuals(models);
    sim::RenderSnapshot snapshot = WithEffect(sim::PowerupKind::Turbo, {.racer = 1}, 1);
    snapshot.racers.resize(2);

    visuals.Update(snapshot, kFrameMs);
    CHECK(visuals.Draws().size() == 1);
    snapshot.racers[1].isGhost = true;
    visuals.Update(snapshot, kFrameMs);
    CHECK(visuals.Draws().empty());
}

TEST_CASE("a warp portal shows only while the warp opens, pulsing in size") {
    const EffectModels models{{"WARPPRT", Static()}};
    PowerupVisuals visuals(models);
    sim::RenderSnapshot opening = WithEffect(sim::PowerupKind::Warp, {.level = 3}, 0);
    opening.effects[0].phaseLeftMs = 250.0F;

    visuals.Update(opening, kFrameMs);
    REQUIRE(visuals.Draws().size() == 1);
    CHECK(visuals.Draws()[0].scale == doctest::Approx(1.0F));
    opening.effects[0].phaseLeftMs = 450.0F;
    visuals.Update(opening, kFrameMs);
    CHECK(visuals.Draws()[0].scale == doctest::Approx(0.309F).epsilon(0.001));
    visuals.Update(WithEffect(sim::PowerupKind::Warp, {.level = 3}, 1), kFrameMs);
    CHECK(visuals.Draws().empty());
}

TEST_CASE("a fading magnet shrinks while its ring fades from half alpha") {
    const EffectModels models{{"MAGNET", Static()}, {"MAGRING", Static()}};
    PowerupVisuals visuals(models);
    sim::RenderSnapshot fading = WithEffect(sim::PowerupKind::Magnet, {.level = 2},
                                            static_cast<std::uint8_t>(sim::MagnetPhase::Fading));
    fading.effects[0].phaseLeftMs = 500.0F;

    visuals.Update(fading, kFrameMs);

    REQUIRE(visuals.Draws().size() == 2);
    CHECK(visuals.Draws()[0].scale == doctest::Approx(0.5F));
    CHECK(visuals.Draws()[0].material.tint.a == 1.0F);
    CHECK(visuals.Draws()[1].scale == 1.0F);
    CHECK(visuals.Draws()[1].material.tint.a == doctest::Approx(0.5F * 127.0F / 255.0F));
    CHECK(visuals.Draws()[1].material.blend == render::BlendMode::Alpha);
}

TEST_CASE("a scaled jointed model shrinks its node offsets too") {
    const JointedAssets jointed = OneNodeShifts({"a0"});
    const EffectModels models{{"WARPPRT", Jointed(jointed)}};
    PowerupVisuals visuals(models);
    sim::RenderSnapshot opening = WithEffect(sim::PowerupKind::Warp, {.level = 3}, 0);
    opening.effects[0].phaseLeftMs = 125.0F;

    visuals.Update(opening, kFrameMs);

    REQUIRE(visuals.Draws().size() == 1);
    CheckNear(GamePosition(visuals.Draws()[0]), kAt + Vector3{.x = 0.70711F});
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
