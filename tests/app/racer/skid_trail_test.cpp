#include "app/racer/skid_trail.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "../../sim/physics/plane_world.hpp"
#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr QuadLook kLook{.texture = render::TextureId{3}, .tint = {.a = 0.5F}};
constexpr float kWidth = 0.3F;

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

Vector3 At(float x) {
    return {.x = x, .z = 1.0F};
}

std::vector<render::QuadDraw> Drawn(const SkidTrail& trail) {
    std::vector<render::QuadDraw> quads;
    trail.Append(test::FlatGround(), quads);
    return quads;
}

TEST_CASE("a skid trail's growing segment settles each quarter of its life, keeping three") {
    SkidTrail trail(kLook, kWidth, 1000.0F);

    trail.Follow(At(0.0F), 0.0F);
    trail.Follow(At(10.0F), 100.0F);
    CHECK(Drawn(trail).size() == 1);
    trail.Follow(At(20.0F), 200.0F);
    const std::vector<render::QuadDraw> settled = Drawn(trail);
    REQUIRE(settled.size() == 1);
    CheckNear(ToGame(settled[0].corners[0]), {.x = 20.0F, .y = 0.15F, .z = 0.1F});
    CheckNear(ToGame(settled[0].corners[3]), {.x = 0.0F, .y = -0.15F, .z = 0.1F});
    for (int i = 3; i < 8; ++i) {
        trail.Follow(At(static_cast<float>(i) * 10.0F), 300.0F);
    }
    trail.Follow(At(85.0F), 10.0F);
    CHECK(Drawn(trail).size() == 4);
}

TEST_CASE("a skid trail ignores moves shorter than a tenth") {
    SkidTrail trail(kLook, kWidth, 250.0F);

    trail.Follow(At(0.0F), 0.0F);
    trail.Follow(At(0.05F), 500.0F);

    CHECK(Drawn(trail).empty());
}

TEST_CASE("a released skid trail fades out whole, or goes at once") {
    SkidTrail trail(kLook, kWidth, 1000.0F);
    trail.Follow(At(0.0F), 0.0F);
    trail.Follow(At(10.0F), 100.0F);

    trail.Release(1000.0F);
    trail.Follow(At(20.0F), 100.0F);
    trail.Fade(250.0F);
    const std::vector<render::QuadDraw> fading = Drawn(trail);
    REQUIRE(fading.size() == 1);
    CHECK(fading[0].tint.a == doctest::Approx(0.375F));
    CheckNear(ToGame(fading[0].corners[0]), {.x = 10.0F, .y = 0.15F, .z = 0.1F});
    trail.Fade(750.0F);
    CHECK(trail.IsGone());
    CHECK(Drawn(trail).empty());

    SkidTrail dropped(kLook, kWidth, 1000.0F);
    dropped.Release(0.0F);
    CHECK(dropped.IsGone());
}

}
}
