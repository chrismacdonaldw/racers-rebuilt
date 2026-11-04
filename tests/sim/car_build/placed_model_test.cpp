#include "sim/car_build/placed_model.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "assets/piece_catalog.hpp"
#include "sim/car_build/car_mesh.hpp"
#include "sim/car_build/car_pieces.hpp"

namespace rr::sim {
namespace {

using test::Block;

constexpr std::uint16_t kBrickType = 0x900;
constexpr float kTolerance = 0.0001F;

bool IsNear(const Vector3& a, const Vector3& b) {
    return Length(a - b) < kTolerance;
}

assets::GeometryAsset OneTriangle(std::string material) {
    return {.vertices = {{.position = {.x = 0.5F}, .normal = {.x = 1.0F}},
                         {.position = {.x = 1.0F, .z = 0.6F}, .normal = {.x = 1.0F}},
                         {.position = {.y = 1.0F}, .normal = {.x = 1.0F}}},
            .indices = {0, 1, 2},
            .materials = {std::move(material)},
            .ranges = {{.firstIndex = 0, .indexCount = 3, .material = 0}}};
}

struct Parts {
    assets::PieceCatalog catalog{.library = {},
                                 .materials = {"unassign", "bottom", "nub", "red", "redaa"},
                                 .colours = {"red"},
                                 .partSets = {}};

    Parts() {
        assets::PieceLibrary& library = catalog.library;
        assets::PieceRecord chassis = test::Chassis();
        chassis.modelOffsetPosition = test::AddPosition(library, {.x = 5.0F, .y = 3.0F});
        library.pieces.push_back(test::WithBox(library, chassis));
        library.pieces.push_back(test::WithBox(library, Block(kBrickType, 1, 2, 3)));
        assets::PieceRecord stud = Block(assets::kStudPieceType, 1, 1, 1);
        stud.indexOffset = static_cast<std::uint32_t>(library.indices.size());
        stud.commandCount = 1;
        library.normals.push_back({.z = 1.0F});
        const auto normal = static_cast<std::uint16_t>(library.normals.size() - 1);
        library.indices.insert(library.indices.end(),
                               {0, test::AddPosition(library, {.z = 0.5F}), normal,
                                test::AddPosition(library, {.x = 1.0F, .z = 0.5F}),
                                test::AddPosition(library, {.y = 1.0F, .z = 0.5F})});
        library.pieces.push_back(stud);
    }

    [[nodiscard]] CarMesh Mesh() const {
        BuiltCar car;
        REQUIRE(car.Place(catalog.library.pieces[0], {}, 0, 0).has_value());
        REQUIRE(car.Place(catalog.library.pieces[1], {.x = 2, .y = 1}, 0, 0).has_value());
        Result<CarMesh> mesh = BuildCarMesh(car, catalog);
        REQUIRE_MESSAGE(mesh.has_value(), mesh.error().message);
        return std::move(*mesh);
    }
};

std::size_t CornersAt(const CarMesh& mesh, float height) {
    return static_cast<std::size_t>(
        std::ranges::count_if(mesh.geometry.indices, [&](std::uint32_t index) {
            return std::abs(mesh.geometry.vertices[index].position.z - height) < kTolerance;
        }));
}

TEST_CASE("a placed model turns like its piece and takes the piece's colour") {
    const assets::PieceRecord piece = Block(kBrickType, 2, 3, 3);
    const std::vector<std::string> materials{"unassign", "red"};
    assets::GeometryAsset geometry;

    AppendPlacedModel(geometry, OneTriangle("PIECE"),
                      {.piece = &piece,
                       .placement = {.x = 1, .y = 2, .rotation = 1},
                       .height = 3,
                       .colourMaterial = 1},
                      {.x = -5.0F}, materials);
    AppendPlacedModel(geometry, OneTriangle("black"), {.piece = &piece, .colourMaterial = 1}, {},
                      materials);

    REQUIRE(geometry.vertices.size() == 6);
    CHECK(IsNear(geometry.vertices[0].position, {.x = -4.0F, .y = 3.5F, .z = 1.2F}));
    CHECK(IsNear(geometry.vertices[1].position, {.x = -4.0F, .y = 3.0F, .z = 1.8F}));
    CHECK(IsNear(geometry.vertices[0].normal, {.y = -1.0F}));
    CHECK(geometry.indices == std::vector<std::uint32_t>{0, 1, 2, 3, 4, 5});
    CHECK(geometry.materials == std::vector<std::string>{"red", "black"});
    REQUIRE(geometry.ranges.size() == 2);
    CHECK(geometry.ranges[1] ==
          assets::GeometryRange{.firstIndex = 3, .indexCount = 3, .material = 1});
}

TEST_CASE_FIXTURE(Parts, "a modelled piece draws its model in place of its mesh and studs") {
    const CarMesh retail = Mesh();
    catalog.models.emplace(std::to_string(kBrickType), OneTriangle("piece"));

    const CarMesh modelled = Mesh();

    const float brickTop = 1.6F;
    const float studTop = 1.8F;
    CHECK(retail.drawnPieces == 2);
    CHECK(modelled.drawnPieces == 2);
    CHECK(CornersAt(retail, brickTop) > 0);
    CHECK(CornersAt(retail, studTop) > 0);
    CHECK(CornersAt(modelled, brickTop) == 0);
    CHECK(CornersAt(modelled, studTop) == 0);
    REQUIRE(modelled.geometry.vertices.size() >= 3);
    const std::size_t first = modelled.geometry.vertices.size() - 3;
    CHECK(IsNear(modelled.geometry.vertices[first].position, {.x = -2.5F, .y = -2.0F, .z = 0.4F}));
    CHECK(std::ranges::count(modelled.geometry.materials, "piece") == 0);
}

TEST_CASE_FIXTURE(Parts, "a modelled piece's preview is its model alone") {
    catalog.models.emplace(std::to_string(kBrickType), OneTriangle("piece"));

    const Result<assets::GeometryAsset> preview =
        BuildPieceMesh(catalog.library.pieces[1], 0, 2, catalog);

    REQUIRE(preview.has_value());
    REQUIRE(preview->vertices.size() == 3);
    CHECK(IsNear(preview->vertices[0].position, {.x = 0.5F, .y = 2.0F}));
    CHECK(preview->materials == std::vector<std::string>{"red"});
}

}
}
