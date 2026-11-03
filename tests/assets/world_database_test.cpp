#include "assets/world_database.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Floats;
using test::Ints;
using test::Join;
using test::ListHeader;
using test::Text;

std::vector<std::byte> Records(std::uint8_t field, const std::vector<std::byte>& name,
                               const std::vector<std::byte>& fields) {
    return Join({Bytes({field}), ListHeader(1), Bytes({field}), name, Bytes({0x05}), fields,
                 Bytes({0x06, 0x06})});
}

std::vector<std::byte> NameLists() {
    std::vector<std::byte> lists;
    for (const std::uint8_t field :
         std::array<std::uint8_t, 9>{0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x3D, 0x40}) {
        lists = Join({lists, Bytes({field}), ListHeader(8)});
        for (int i = 0; i < 8; ++i) {
            lists = Join({lists, Text("N")});
        }
        lists = Join({lists, Bytes({0x06})});
    }
    return lists;
}

Result<WorldAsset> ParseEntity(std::uint8_t field, const std::vector<std::byte>& fields) {
    return ParseWorld(Join({NameLists(), Records(field, Text("e"), fields)}));
}

TEST_CASE("name lists fill their tables") {
    const Result<WorldAsset> world = ParseWorld(Join({
        Bytes({0x2A}),
        ListHeader(2),
        Text("TREE"),
        Text("ROCK"),
        Bytes({0x06, 0x40}),
        ListHeader(1),
        Text("BOX"),
        Bytes({0x06}),
    }));

    REQUIRE(world.has_value());
    CHECK(world->models == std::vector<std::string>{"TREE", "ROCK"});
    CHECK(world->boundingVolumes == std::vector<std::string>{"BOX"});
}

TEST_CASE("model entities read placement, levels of detail and decoration") {
    const Result<WorldAsset> world = ParseEntity(0x2E, Join({
                                                           Bytes({0x2A}),
                                                           Ints({1}),
                                                           Floats({-1}),
                                                           Bytes({0x31}),
                                                           Floats({1, 2, 3}),
                                                           Bytes({0x32}),
                                                           Floats({0, 1, 0, 0, 0, -1}),
                                                           Bytes({0x2B}),
                                                           Ints({4, 2}),
                                                           Bytes({0x36}),
                                                           Floats({2}),
                                                           Bytes({0x3E}),
                                                           ListHeader(1),
                                                           Ints({5, 6, 7, 8}),
                                                           Bytes({0x06, 0x3F}),
                                                           Floats({3, 4}),
                                                           Bytes({0x42, 0x4C}),
                                                       }));

    REQUIRE(world.has_value());
    REQUIRE(world->modelEntities.size() == 1);
    const WorldEntity& entity = world->modelEntities.front();
    CHECK(entity.name == "e");
    CHECK(entity.lods == std::vector<WorldLod>{{.model = 1}});
    CHECK(entity.lods.front().maxDistance == std::numeric_limits<float>::infinity());
    CHECK(entity.position == Vector3{.x = 1.0F, .y = 2.0F, .z = 3.0F});
    CHECK(entity.forward == Vector3{.y = 1.0F});
    CHECK(entity.up == Vector3{.z = -1.0F});
    CHECK(entity.materialTables == std::array<std::int32_t, 3>{kNoIndex, kNoIndex, 4});
    CHECK(entity.scale == 2.0F);
    CHECK(entity.materialAnimations ==
          std::vector<MaterialAnimationBinding>{
              {.animation = 5, .track = 6, .material = 7, .materialTable = 8}});
    CHECK(entity.textureScroll == TextureScroll{.uPerSecond = 3.0F, .vPerSecond = 4.0F});
    CHECK(entity.hasFlag42);
    CHECK(entity.isEnvironmentMapped);
}

TEST_CASE("an entity without fields keeps the defaults") {
    const Result<WorldAsset> world = ParseEntity(0x2E, {});

    REQUIRE(world.has_value());
    const WorldEntity& entity = world->modelEntities.front();
    CHECK(entity.lods.empty());
    CHECK(entity.forward == Vector3{.x = 1.0F});
    CHECK(entity.up == Vector3{.z = 1.0F});
    CHECK(entity.scale == 1.0F);
    CHECK_FALSE(entity.textureScroll.has_value());
    CHECK_FALSE(entity.startPart.has_value());
}

TEST_CASE("animated entities read jointed and node levels of detail") {
    const Result<WorldAsset> world = ParseEntity(0x2F, Join({
                                                           Bytes({0x33}),
                                                           Ints({1, 2, 3}),
                                                           Floats({10}),
                                                           Bytes({0x2C}),
                                                           Ints({4, 5}),
                                                           Floats({20}),
                                                           Bytes({0x35}),
                                                           Text("HEAD"),
                                                       }));

    REQUIRE(world.has_value());
    const WorldEntity& entity = world->animatedEntities.front();
    CHECK(entity.lods ==
          std::vector<WorldLod>{{.model = 1, .sceneNode = 2, .modelPart = 3, .maxDistance = 10.0F},
                                {.sceneNode = 4, .modelPart = 5, .maxDistance = 20.0F}});
    CHECK(entity.startPart == NameOrIndex{.name = "HEAD"});
}

TEST_CASE("a start part can be an index") {
    const Result<WorldAsset> world = ParseEntity(0x2F, Join({Bytes({0x35}), Ints({7})}));

    REQUIRE(world.has_value());
    CHECK(world->animatedEntities.front().startPart == NameOrIndex{.index = 7});
    CHECK_FALSE(ParseEntity(0x2F, Join({Bytes({0x35}), Floats({7})})).has_value());
}

TEST_CASE("an entity holds at most three levels of detail") {
    const std::vector<std::byte> lod = Join({Bytes({0x2A}), Ints({0}), Floats({1})});

    CHECK(ParseEntity(0x2E, Join({lod, lod, lod})).has_value());
    CHECK_FALSE(ParseEntity(0x2E, Join({lod, lod, lod, lod})).has_value());
}

TEST_CASE("a material table slot past 2 fails") {
    CHECK_FALSE(ParseEntity(0x2E, Join({Bytes({0x2B}), Ints({0, 3})})).has_value());
}

TEST_CASE("fields of another entity kind fail") {
    CHECK_FALSE(ParseEntity(0x2E, Join({Bytes({0x35}), Ints({0})})).has_value());
    CHECK_FALSE(ParseEntity(0x30, Join({Bytes({0x36}), Floats({1})})).has_value());
}

TEST_CASE("collidable and bounded entities read their shapes") {
    const Result<WorldAsset> collidable =
        ParseEntity(0x30, Join({Bytes({0x34}), Ints({1, 2}), Floats({5})}));
    const Result<WorldAsset> bounded =
        ParseEntity(0x41, Join({Bytes({0x40}), Ints({3}), Bytes({0x2B}), Ints({4})}));

    REQUIRE(collidable.has_value());
    REQUIRE(bounded.has_value());
    CHECK(collidable->collidableEntities.front().lods ==
          std::vector<WorldLod>{{.model = 1, .boundingShape = 2, .maxDistance = 5.0F}});
    const WorldEntity& entity = bounded->boundedEntities.front();
    CHECK(entity.boundingVolume == 3);
    CHECK(entity.materialTables.front() == 4);
}

TEST_CASE("records may leave out their name") {
    const Result<WorldAsset> world = ParseWorld(Records(0x2E, {}, {}));

    REQUIRE(world.has_value());
    CHECK(world->modelEntities.front().name.empty());
}

TEST_CASE("sprites read their billboard") {
    const std::vector<std::byte> fields = Join({
        Bytes({0x2B}),
        Ints({1, 2}),
        Bytes({0x31}),
        Floats({1, 2, 3}),
        Bytes({0x38}),
        Floats({0, 0, 1}),
        Bytes({0x39}),
        Text("FLAME"),
        Bytes({0x3A}),
        Floats({4}),
        Bytes({0x3B}),
        Floats({5}),
        Bytes({0x3C}),
        Floats({6}),
        Bytes({0x3E}),
        Ints({7, 8}),
    });
    const Result<WorldAsset> world = ParseWorld(Join({NameLists(), Records(0x37, {}, fields)}));

    REQUIRE(world.has_value());
    const WorldSprite& sprite = world->sprites.front();
    CHECK(sprite.materialName == "FLAME");
    CHECK(sprite.material == SpriteMaterial{.materialTable = 1, .material = 2});
    CHECK(sprite.animation == SpriteAnimation{.animation = 7, .track = 8});
    CHECK(sprite.position == Vector3{.x = 1.0F, .y = 2.0F, .z = 3.0F});
    CHECK(sprite.axis == Vector3{.z = 1.0F});
    CHECK(sprite.width == 4.0F);
    CHECK(sprite.height == 5.0F);
    CHECK(sprite.maxDistance == 6.0F);
}

TEST_CASE("sprites have no name") {
    CHECK_FALSE(ParseWorld(Records(0x37, Text("s"), {})).has_value());
}

TEST_CASE("cameras read their lens and target") {
    const std::vector<std::byte> fields =
        Join({Bytes({0x2F}), Ints({0, 2}), Bytes({0x44}), Floats({0, 5, 0}), Bytes({0x45}),
              Floats({2}), Bytes({0x47}), Floats({90})});
    const Result<WorldAsset> world =
        ParseWorld(Join({Records(0x2F, {}, {}), Records(0x43, Text("c"), fields)}));

    REQUIRE(world.has_value());
    const WorldCamera& camera = world->cameras.front();
    CHECK(camera.name == "c");
    CHECK(camera.trackedEntity == 0);
    CHECK(camera.trackedNode == 2);
    CHECK(camera.target == Vector3{.y = 5.0F});
    CHECK(camera.nearClip == 2.0F);
    CHECK(camera.farClip == 100.0F);
    CHECK(camera.fovDegrees == 90.0F);
}

TEST_CASE("lights read their color and direction") {
    const Result<WorldAsset> world = ParseWorld(Join({
        Records(0x48, Text("sky"), Join({Bytes({0x4A}), Ints({10, 20, 30})})),
        Records(0x49, {},
                Join({Bytes({0x4A}), Ints({1, 2, 3}), Bytes({0x4B}), Floats({0, 0, -1})})),
    }));

    REQUIRE(world.has_value());
    CHECK(world->ambientLights.front().name == "sky");
    CHECK(world->ambientLights.front().color == Rgba8{10, 20, 30, 255});
    CHECK(world->lights.front().color == Rgba8{1, 2, 3, 255});
    CHECK(world->lights.front().direction == Vector3{.z = -1.0F});
    CHECK_FALSE(
        ParseWorld(Records(0x48, {}, Join({Bytes({0x4B}), Floats({0, 0, 1})}))).has_value());
}

std::vector<std::byte> TwoModels() {
    return Join({Bytes({0x2A}), ListHeader(2), Text("A"), Text("B"), Bytes({0x06})});
}

TEST_CASE("indices must fall inside their lists") {
    const std::vector<std::byte> model1 =
        Records(0x2E, {}, Join({Bytes({0x2A}), Ints({1}), Floats({1})}));
    const std::vector<std::byte> model2 =
        Records(0x2E, {}, Join({Bytes({0x2A}), Ints({2}), Floats({1})}));

    CHECK(ParseWorld(Join({TwoModels(), model1})).has_value());
    CHECK_FALSE(ParseWorld(Join({TwoModels(), model2})).has_value());
}

TEST_CASE("indices are checked after the whole file is read") {
    const std::vector<std::byte> model1 =
        Records(0x2E, {}, Join({Bytes({0x2A}), Ints({1}), Floats({1})}));

    CHECK(ParseWorld(Join({model1, TwoModels()})).has_value());
    CHECK_FALSE(ParseWorld(model1).has_value());
}

TEST_CASE("a camera must track an existing animated entity") {
    const std::vector<std::byte> camera = Records(0x43, {}, Join({Bytes({0x2F}), Ints({0, 0})}));
    const std::vector<std::byte> animated = Records(0x2F, {}, {});

    CHECK(ParseWorld(Join({camera, animated})).has_value());
    CHECK_FALSE(ParseWorld(camera).has_value());
}

TEST_CASE("node references and binding tables are not checked") {
    CHECK(ParseEntity(0x2F, Join({Bytes({0x35}), Ints({3})})).has_value());
    const std::vector<std::byte> animations =
        Join({Bytes({0x3D}), ListHeader(1), Text("GLOW"), Bytes({0x06})});
    const std::vector<std::byte> binding =
        Records(0x2E, {}, Join({Bytes({0x3E}), ListHeader(1), Ints({0, 0, 0, 5}), Bytes({0x06})}));

    CHECK(ParseWorld(Join({animations, binding})).has_value());
    CHECK_FALSE(ParseWorld(binding).has_value());
}

TEST_CASE("the draw range is the farthest level's distance, squared") {
    constexpr float kNoLimit = std::numeric_limits<float>::infinity();
    WorldEntity entity;
    CHECK(DrawRangeSquared(entity) == kNoLimit);

    entity.lods = {WorldLod{.maxDistance = 300.0F}, WorldLod{.maxDistance = 20.0F}};
    CHECK(DrawRangeSquared(entity) == 90000.0F);

    entity.lods.push_back(WorldLod{});
    CHECK(DrawRangeSquared(entity) == kNoLimit);
}

}
}
