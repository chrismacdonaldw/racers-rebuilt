#include "app/track_sprites.hpp"

#include <doctest/doctest.h>

#include <memory>
#include <vector>

#include "core/quaternion.hpp"

namespace rr::app {
namespace {

assets::NameMap<render::Material> Materials() {
    return {{"ADTORCH", render::Material{.texture = render::TextureId{4}}},
            {"WELDER", render::Material{.texture = render::TextureId{5}}},
            {"WELDER2", render::Material{.texture = render::TextureId{6}}}};
}

assets::WorldSprite Torch(float maxDistance) {
    return {.materialName = "adtorch",
            .position = {.y = 10.0F},
            .axis = Vector3{.z = 2.0F},
            .width = 4.0F,
            .height = 2.0F,
            .maxDistance = maxDistance};
}

assets::TrackAssets WeldingTrack() {
    assets::TrackAssets track;
    track.models.emplace("SPRITE", std::make_shared<const assets::GeometryAsset>(
                                       assets::GeometryAsset{.materials = {"gold", "welder"}}));
    track.materialAnimations.emplace(
        "WELD",
        std::make_shared<const assets::MaterialAnimationAsset>(assets::MaterialAnimationAsset{
            .keys = {{.material = "welder", .frame = 0}, {.material = "welder2", .frame = 1}},
            .tracks = {{.firstKey = 0, .keyCount = 2, .duration = 2}}}));
    return track;
}

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

TEST_CASE("a track sprite stands on its axis in engine space with its named material") {
    TrackSprites sprites;
    sprites.Add({.sprites = {Torch(100.0F)}}, {}, Materials());
    std::vector<render::QuadDraw> quads;

    sprites.Append({.rotation = FromAxisAngle({.x = 1.0F}, -0.5F)}, true, 0.0F, quads);

    REQUIRE(quads.size() == 1);
    CHECK(quads[0].texture == render::TextureId{4});
    CheckNear(quads[0].corners[0], {.x = -2.0F, .y = 1.0F, .z = -10.0F});
    CheckNear(quads[0].corners[3], {.x = 2.0F, .y = -1.0F, .z = -10.0F});
}

TEST_CASE("a track sprite past its draw distance is left out only while far scenery is hidden") {
    TrackSprites sprites;
    sprites.Add({.sprites = {Torch(9.0F), Torch(10.5F)}}, {}, Materials());
    std::vector<render::QuadDraw> hidden;
    std::vector<render::QuadDraw> shown;

    sprites.Append({}, true, 0.0F, hidden);
    sprites.Append({}, false, 0.0F, shown);

    CHECK(hidden.size() == 1);
    CHECK(shown.size() == 2);
}

TEST_CASE("a table-material sprite plays its animation from its table's material") {
    TrackSprites sprites;
    assets::WorldSprite welder = Torch(100.0F);
    welder.material = assets::SpriteMaterial{.materialTable = 0, .material = 1};
    welder.animation = assets::SpriteAnimation{.animation = 0, .track = 0};
    sprites.Add({.materialTables = {"sprite"}, .materialAnimations = {"weld"}, .sprites = {welder}},
                WeldingTrack(), Materials());
    std::vector<render::QuadDraw> first;
    std::vector<render::QuadDraw> second;

    sprites.Append({}, false, 0.0F, first);
    sprites.Append({}, false, 40.0F, second);

    REQUIRE(first.size() == 1);
    REQUIRE(second.size() == 1);
    CHECK(first[0].texture == render::TextureId{5});
    CHECK(second[0].texture == render::TextureId{6});
}

TEST_CASE("sprites whose named or table material is missing are left out") {
    TrackSprites sprites;
    assets::WorldSprite pastTable = Torch(100.0F);
    pastTable.material = assets::SpriteMaterial{.materialTable = 0, .material = 2};
    assets::WorldSprite missing = Torch(100.0F);
    missing.materialName = "nothing";
    sprites.Add({.materialTables = {"sprite"}, .sprites = {pastTable, missing}}, WeldingTrack(),
                Materials());
    std::vector<render::QuadDraw> quads;

    sprites.Append({}, false, 0.0F, quads);

    CHECK(quads.empty());
}

}
}
