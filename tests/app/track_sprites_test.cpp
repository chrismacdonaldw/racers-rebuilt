#include "app/track_sprites.hpp"

#include <doctest/doctest.h>

#include <array>
#include <vector>

#include "core/quaternion.hpp"

namespace rr::app {
namespace {

assets::NameMap<render::Material> Materials() {
    return {{"ADTORCH", render::Material{.texture = render::TextureId{4}}}};
}

assets::WorldSprite Torch(float maxDistance) {
    return {.materialName = "adtorch",
            .position = {.y = 10.0F},
            .axis = Vector3{.z = 2.0F},
            .width = 4.0F,
            .height = 2.0F,
            .maxDistance = maxDistance};
}

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

TEST_CASE("a track sprite stands on its axis in engine space with its named material") {
    TrackSprites sprites;
    const std::array<assets::WorldSprite, 1> world{Torch(100.0F)};
    sprites.Add(world, Materials());
    std::vector<render::QuadDraw> quads;

    sprites.Append({.rotation = FromAxisAngle({.x = 1.0F}, -0.5F)}, true, quads);

    REQUIRE(quads.size() == 1);
    CHECK(quads[0].texture == render::TextureId{4});
    CheckNear(quads[0].corners[0], {.x = -2.0F, .y = 1.0F, .z = -10.0F});
    CheckNear(quads[0].corners[3], {.x = 2.0F, .y = -1.0F, .z = -10.0F});
}

TEST_CASE("a track sprite past its draw distance is left out only while far scenery is hidden") {
    TrackSprites sprites;
    const std::array<assets::WorldSprite, 2> world{Torch(9.0F), Torch(10.5F)};
    sprites.Add(world, Materials());
    std::vector<render::QuadDraw> hidden;
    std::vector<render::QuadDraw> shown;

    sprites.Append({}, true, hidden);
    sprites.Append({}, false, shown);

    CHECK(hidden.size() == 1);
    CHECK(shown.size() == 2);
}

TEST_CASE("table-material sprites and sprites with a missing material are left out") {
    TrackSprites sprites;
    assets::WorldSprite table = Torch(100.0F);
    table.material = assets::SpriteMaterial{};
    assets::WorldSprite missing = Torch(100.0F);
    missing.materialName = "nothing";
    const std::array<assets::WorldSprite, 2> world{table, missing};
    sprites.Add(world, Materials());
    std::vector<render::QuadDraw> quads;

    sprites.Append({}, false, quads);

    CHECK(quads.empty());
}

}
}
