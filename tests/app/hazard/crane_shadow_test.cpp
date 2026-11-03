#include "app/hazard/crane_shadow.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <numbers>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "../../sim/hazard/hazard_rig.hpp"
#include "../decal_ground.hpp"
#include "app/track_space.hpp"
#include "core/quaternion.hpp"
#include "sim/hazard/crane.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kBall{.x = 30.0F, .y = 20.0F, .z = 4.0F};
constexpr QuadLook kLook{.texture = render::TextureId{7}};
constexpr float kHalfSpan = 6.5F / 0.99F;

sim::TrackEntitySources CraneEntity(const Quaternion& rotation = {}) {
    sim::TrackEntitySources sources = test::StillEntities({"crane"}, kBall);
    sources.entities[0].placement.rotation = rotation;
    sources.entities[0].drawRangeSquared = 100.0F * 100.0F;
    return sources;
}

struct CraneRig {
    explicit CraneRig(sim::TrackEntitySources entities)
        : sources(std::move(entities)), rig({{.kind = assets::HazardKind::Crane}}, sources) {}

    [[nodiscard]] std::vector<render::QuadDraw> Cast(const CraneShadow& shadow,
                                                     std::span<const Vector3> cameras = {}) const {
        GroundDecals ground(surface);
        std::vector<render::QuadDraw> quads;
        shadow.Append(rig.Hazards(), rig.Entities().Clocks(), cameras, ground, quads);
        return quads;
    }

    sim::TrackEntitySources sources;
    test::HazardRig rig;
    DecalSurface surface = test::FlatDecalGround();
};

void CheckSquareUnderBall(const render::QuadDraw& quad) {
    for (const Vector3& corner : quad.corners) {
        const Vector3 game = ToGame(corner);
        CHECK(std::abs(game.x - kBall.x) == doctest::Approx(kHalfSpan).epsilon(test::kClipEpsilon));
        CHECK(std::abs(game.y - kBall.y) == doctest::Approx(kHalfSpan).epsilon(test::kClipEpsilon));
        CHECK(game.z == doctest::Approx(0.1F));
    }
}

Vector3 FarthestAlongTexture(const render::QuadDraw& quad) {
    REQUIRE(quad.cornerUvs.has_value());
    const auto& uvs = *quad.cornerUvs;
    const auto most = std::ranges::max_element(
        uvs, {}, [](const render::TexturePoint& point) { return point.v; });
    return ToGame(quad.corners.at(static_cast<std::size_t>(std::distance(uvs.begin(), most))));
}

}

TEST_CASE("the crane casts a 13 unit square shadow under its ball once it runs") {
    CraneRig crane(CraneEntity());
    const CraneShadow shadow(kLook, crane.sources);
    CHECK(crane.Cast(shadow).empty());

    crane.rig.Hear(sim::kCraneTrigger, sim::EventEdge::Start);
    const std::vector<render::QuadDraw> quads = crane.Cast(shadow);

    REQUIRE(quads.size() == 1);
    CHECK(quads[0].texture == render::TextureId{7});
    CheckSquareUnderBall(quads[0]);
    CHECK(FarthestAlongTexture(quads[0]).x > kBall.x);
}

TEST_CASE("the crane's shadow texture runs along the crane's forward axis") {
    CraneRig crane(CraneEntity(FromAxisAngle({.z = 1.0F}, std::numbers::pi_v<float> / 2.0F)));
    const CraneShadow shadow(kLook, crane.sources);
    crane.rig.Hear(sim::kCraneTrigger, sim::EventEdge::Start);

    const std::vector<render::QuadDraw> quads = crane.Cast(shadow);

    REQUIRE(quads.size() == 1);
    CHECK(FarthestAlongTexture(quads[0]).y > kBall.y);
}

TEST_CASE("the crane's shadow shows only while a camera is within the crane's draw range") {
    CraneRig crane(CraneEntity());
    const CraneShadow shadow(kLook, crane.sources);
    crane.rig.Hear(sim::kCraneTrigger, sim::EventEdge::Start);
    const std::vector<Vector3> far{ToEngine(kBall + Vector3{.x = 100.0F})};
    const std::vector<Vector3> near{ToEngine(kBall + Vector3{.x = 100.0F}),
                                    ToEngine(kBall + Vector3{.y = 99.0F})};

    CHECK(crane.Cast(shadow, far).empty());
    CHECK(crane.Cast(shadow, near).size() == 1);
}

TEST_CASE("a track without the crane shadow material casts none") {
    CraneRig crane(CraneEntity());
    const CraneShadow shadow(std::nullopt, crane.sources);
    crane.rig.Hear(sim::kCraneTrigger, sim::EventEdge::Start);

    CHECK(crane.Cast(shadow).empty());
}

}
