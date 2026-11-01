#include "app/powerup/warp_tube.hpp"

#include <doctest/doctest.h>

#include <memory>
#include <vector>

#include "app/chase_camera.hpp"
#include "app/track_space.hpp"
#include "sim/powerup/warp_effect.hpp"

namespace rr::app {
namespace {

constexpr float kModelScale = 1.0F / 64.0F;
constexpr Vector3 kPlaced{.x = 1.0F, .y = 9.0F, .z = -147.0F};

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

JointedAssets RisesOneUnit() {
    JointedAssets jointed{
        .skeleton = {.nodes = {{.name = "root", .position = {}, .rotation = {}, .parent = {}}}},
        .animations = {}};
    jointed.animations.positionKeys.push_back({.z = 64.0F});
    jointed.animations.tracks.push_back(
        {.rotations = {}, .positions = {.firstValue = 0, .count = 1}});
    jointed.animations.parts.push_back({.name = "a0",
                                        .firstTrack = 0,
                                        .frameCount = 45,
                                        .loopFrames = 45,
                                        .framesPerSecond = 30,
                                        .velocity = {},
                                        .bounds = {}});
    return jointed;
}

WarpTube TubeOf(const JointedAssets& jointed) {
    EffectModels models;
    models.emplace("DTUBE", EffectModel{.draws = {.draws = std::vector<render::MeshDraw>(1),
                                                  .joints = {0},
                                                  .scale = kModelScale},
                                        .jointed = &jointed,
                                        .placement = {.position = kPlaced},
                                        .scroll = {.uPerSecond = -1.0F, .vPerSecond = 5.0F}});
    return {nullptr, std::move(models)};
}

sim::EffectSnapshot Warp(std::size_t racer, sim::WarpPhase phase) {
    return {.kind = sim::PowerupKind::Warp,
            .use = {.racer = static_cast<std::uint8_t>(racer), .level = 3},
            .phase = static_cast<std::uint8_t>(phase)};
}

TEST_CASE("the warp tube draws at its placement, animated and scrolled by the time flown") {
    const JointedAssets jointed = RisesOneUnit();
    WarpTube tube = TubeOf(jointed);

    tube.Fly(1400.0F, {}, {});

    REQUIRE(tube.Draws().size() == 1);
    const render::MeshDraw& draw = tube.Draws()[0];
    CheckNear(TransformToGame(draw.transform).position, kPlaced + Vector3{.z = 1.0F});
    CHECK(draw.material.uvOffset.u == doctest::Approx(0.9F));
    CHECK(draw.material.uvOffset.v == doctest::Approx(0.5F));
}

TEST_CASE("the warping car moves from its pose to the tube's origin, facing game +X") {
    const JointedAssets jointed = RisesOneUnit();
    WarpTube tube = TubeOf(jointed);
    const Transform car = TransformToEngine({.position = {.x = 300.0F, .y = -40.0F, .z = 12.0F},
                                             .rotation = FromAxisAngle({.z = 1.0F}, 1.2F)});
    const Transform wheel{.position = {.x = 2.0F, .y = 1.0F}};
    const std::vector<render::MeshDraw> draws{{.transform = Compose(car, wheel)}};

    tube.Fly(700.0F, car, draws);

    REQUIRE(tube.Draws().size() == 2);
    CHECK(Length(tube.Draws()[1].transform.position - wheel.position) < 0.001F);
    CHECK(Length(Rotate(tube.Draws()[1].transform.rotation, {.x = 1.0F}) - Vector3{.x = 1.0F}) <
          0.001F);
}

TEST_CASE("only the racer's own warp counts as a flight, and only once it is under way") {
    const std::vector<sim::EffectSnapshot> effects{Warp(0, sim::WarpPhase::Starting),
                                                   Warp(1, sim::WarpPhase::Active),
                                                   {.kind = sim::PowerupKind::Turbo}};

    CHECK(FindWarpFlight(effects, 0) == nullptr);
    CHECK(FindWarpFlight(effects, 1) == &effects[1]);
    CHECK(FindWarpFlight(effects, 2) == nullptr);
}

TEST_CASE("the flight camera chases the car in the tube and swells 45 degrees mid-flight") {
    const render::Camera base{.verticalFovDegrees = 60.0F};
    const Transform car = CarToEngine({});

    CHECK(WarpFlightCamera(base, 1500.0F).verticalFovDegrees == doctest::Approx(60.0F));
    CHECK(WarpFlightCamera(base, 750.0F).verticalFovDegrees == doctest::Approx(105.0F));
    CHECK(WarpFlightCamera(base, 0.0F).verticalFovDegrees == doctest::Approx(60.0F));
    CheckNear(WarpFlightCamera(base, 750.0F).transform.position,
              ChaseCamera(car).Pose(car).position);
}

}
}
