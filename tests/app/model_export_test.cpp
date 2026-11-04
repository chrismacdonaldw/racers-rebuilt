#include "app/model_export.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "assets/geometry_writer.hpp"
#include "assets/gltf_model.hpp"
#include "assets/token_bytes.hpp"
#include "game_layers.hpp"

namespace rr::app {
namespace {

using test::Bytes;
using test::Floats;
using test::Join;
using test::NamedEntry;
using test::NamedLibrary;
using test::Text;

constexpr std::byte kEntry{0x27};

std::vector<std::byte> Model(std::uint16_t joint) {
    const assets::GeometryAsset geometry{
        .vertices = {{.normal = {.z = 1.0F}, .joint = joint},
                     {.position = {.x = 1.0F}, .normal = {.z = 1.0F}, .joint = joint},
                     {.position = {.y = 1.0F}, .normal = {.z = 1.0F}, .joint = joint}},
        .indices = {0, 1, 2},
        .materials = {"STONE"},
        .ranges = {{.indexCount = 3, .joint = joint}}};
    Result<std::vector<std::byte>> gdb = assets::WriteGeometry(geometry, assets::GdbVertices::Lit);
    REQUIRE(gdb.has_value());
    return std::move(*gdb);
}

std::vector<std::byte> Pelvis() {
    return NamedLibrary(
        0x27, {NamedEntry(0x27, "root", {}),
               NamedEntry(0x27, "leg",
                          Join({Bytes({0x28}), Floats({0, 0, 45}), Bytes({0x2A}), Text("root")}))});
}

class ExportFixture {
public:
    ExportFixture() {
        layers_.WriteGame("GAMEDATA/COMMON/ROCK.GDB", Model(0));
        layers_.WriteGame("GAMEDATA/COMMON/BROKEN.GDB", {kEntry});
        layers_.WriteGame("GAMEDATA/DRIVER/RRPELVIS.GDB", Model(1));
        layers_.WriteGame("GAMEDATA/DRIVER/PELVIS.SDB", Pelvis());
        layers_.WriteLoose("LOOSE.GDB", Model(0));
        const std::unique_ptr<assets::Vfs> vfs = layers_.Mount();
        REQUIRE(ExportGameModels(*vfs, test::GameLayers::kGameLayer, Out()).has_value());
    }

    [[nodiscard]] std::filesystem::path Out() const { return layers_.Mods() / "models"; }

private:
    test::GameLayers layers_;
};

assets::GltfModel Import(const std::filesystem::path& path, const assets::GltfTarget& target) {
    Result<assets::GltfModel> model = assets::ImportGltfModel(test::ReadFileBytes(path), target);
    REQUIRE(model.has_value());
    return std::move(*model);
}

TEST_CASE("each game model is exported at its game path as a .GLB") {
    const ExportFixture fixture;

    const assets::GltfModel rock = Import(fixture.Out() / "GAMEDATA/COMMON/ROCK.GLB", {});

    CHECK(rock.geometry.materials == std::vector<std::string>{"STONE"});
    CHECK(rock.geometry.vertices.size() == 3);
}

TEST_CASE("a driver part is exported with the shared pelvis skeleton") {
    const ExportFixture fixture;

    const assets::GltfModel part =
        Import(fixture.Out() / "GAMEDATA/DRIVER/RRPELVIS.GLB", {.joints = {"ROOT", "LEG"}});

    REQUIRE(part.geometry.vertices.size() == 3);
    CHECK(part.geometry.vertices[0].joint == 1);
}

TEST_CASE("models that don't parse and files outside the game layer are skipped") {
    const ExportFixture fixture;

    CHECK_FALSE(std::filesystem::exists(fixture.Out() / "GAMEDATA/COMMON/BROKEN.GLB"));
    CHECK_FALSE(std::filesystem::exists(fixture.Out() / "LOOSE.GLB"));
}

}
}
