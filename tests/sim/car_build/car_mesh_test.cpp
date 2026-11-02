#include "sim/car_build/car_mesh.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sim/car_build/car_pieces.hpp"
#include "sim/car_build/retail_cars.hpp"

namespace rr::sim {
namespace {

using test::Block;

constexpr std::uint16_t kNubCommand = 2;
constexpr std::uint16_t kCulledCommand = 1;
constexpr float kPlate = 0.4F;

struct Parts {
    assets::PieceCatalog catalog{.library = {},
                                 .materials = {"unassign", "bottom", "nub", "red", "redaa"},
                                 .colours = {"red"},
                                 .partSets = {}};

    void Add(const assets::PieceRecord& piece) { catalog.library.pieces.push_back(piece); }

    void AddChassis(bool hasBox) {
        assets::PieceRecord chassis = test::Chassis();
        chassis.modelOffsetPosition =
            test::AddPosition(catalog.library, {.x = 5.0F, .y = 3.0F, .z = 0.0F});
        Add(hasBox ? test::WithBox(catalog.library, chassis) : chassis);
    }

    void AddStud(std::uint16_t triangles) {
        assets::PieceLibrary& library = catalog.library;
        assets::PieceRecord stud = Block(assets::kStudPieceType, 1, 1, 1);
        stud.indexOffset = static_cast<std::uint32_t>(library.indices.size());
        stud.commandCount = triangles;
        const std::uint16_t corner = test::AddPosition(library, {.z = 1.0F});
        library.normals.push_back({.z = 1.0F});
        const auto normal = static_cast<std::uint16_t>(library.normals.size() - 1);
        for (std::uint16_t triangle = 0; triangle < triangles; ++triangle) {
            library.indices.insert(library.indices.end(), {0, corner, normal, corner, corner});
        }
        Add(stud);
    }

    [[nodiscard]] const assets::PieceRecord& Last() const { return catalog.library.pieces.back(); }

    [[nodiscard]] BuiltCar Car(const std::vector<PiecePlacement>& bricks) const {
        BuiltCar car;
        REQUIRE(car.Place(catalog.library.pieces.front(), {}, 0, 0).has_value());
        for (const PiecePlacement& placement : bricks) {
            REQUIRE(car.Place(Last(), placement, 0, 0).has_value());
        }
        return car;
    }

    [[nodiscard]] CarMesh Mesh(const std::vector<PiecePlacement>& bricks) const {
        Result<CarMesh> mesh = BuildCarMesh(Car(bricks), catalog);
        REQUIRE_MESSAGE(mesh.has_value(), mesh.error().message);
        return std::move(*mesh);
    }
};

std::size_t Triangles(const CarMesh& mesh) {
    return mesh.geometry.indices.size() / 3;
}

std::array<Vector3, 3> Corners(const CarMesh& mesh, std::size_t triangle) {
    const auto at = [&](std::size_t corner) {
        return mesh.geometry.vertices[mesh.geometry.indices[(triangle * 3) + corner]].position;
    };
    return {at(0), at(1), at(2)};
}

float FlatArea(const CarMesh& mesh, float height) {
    float area = 0.0F;
    for (std::size_t triangle = 0; triangle < Triangles(mesh); ++triangle) {
        const std::array<Vector3, 3> c = Corners(mesh, triangle);
        if (std::ranges::all_of(c,
                                [&](const Vector3& p) { return p.z == doctest::Approx(height); })) {
            area += std::abs(((c[1].x - c[0].x) * (c[2].y - c[0].y)) -
                             ((c[2].x - c[0].x) * (c[1].y - c[0].y))) /
                    2.0F;
        }
    }
    return area;
}

bool HasWallAt(const CarMesh& mesh, float x) {
    for (std::size_t triangle = 0; triangle < Triangles(mesh); ++triangle) {
        const std::array<Vector3, 3> c = Corners(mesh, triangle);
        if (std::ranges::all_of(c, [&](const Vector3& p) { return p.x == doctest::Approx(x); })) {
            return true;
        }
    }
    return false;
}

TEST_CASE_FIXTURE(Parts, "two touching bricks lose the faces they share") {
    AddChassis(false);
    Add(test::WithBox(catalog.library, Block(0x900, 1, 1, 3)));

    const CarMesh apart = Mesh({{.x = 2, .y = 1}, {.x = 4, .y = 1}});
    const CarMesh touching = Mesh({{.x = 2, .y = 1}, {.x = 3, .y = 1}});

    CHECK(Triangles(apart) == 24);
    CHECK(HasWallAt(apart, -2.0F));
    CHECK(Triangles(touching) == 20);
    CHECK_FALSE(HasWallAt(touching, -2.0F));
    CHECK(HasWallAt(touching, -3.0F));
}

TEST_CASE_FIXTURE(Parts, "a brick cuts its footprint out of the chassis top") {
    AddChassis(true);
    Add(test::WithBox(catalog.library, Block(0x900, 1, 1, 3)));

    const CarMesh mesh = Mesh({{.x = 2, .y = 1}});

    CHECK(Triangles(mesh) == 28);
    CHECK(FlatArea(mesh, kPlate) == doctest::Approx(59.0F));
    CHECK(FlatArea(mesh, 4 * kPlate) == doctest::Approx(1.0F));
    CHECK(FlatArea(mesh, 0.0F) == doctest::Approx(60.0F));
}

TEST_CASE_FIXTURE(Parts, "bricks turn with their placement and sit at their height") {
    AddChassis(false);
    Add(test::WithBox(catalog.library, Block(0x900, 2, 1, 3)));

    const CarMesh mesh = Mesh({{.x = 2, .y = 1, .rotation = 1}});

    const auto [low, high] = std::ranges::minmax(
        mesh.geometry.vertices, {},
        [](const auto& vertex) { return vertex.position.x + (vertex.position.y * 100.0F); });
    CHECK(low.position.x == doctest::Approx(-3.0F));
    CHECK(low.position.y == doctest::Approx(-2.0F));
    CHECK(high.position.x == doctest::Approx(-2.0F));
    CHECK(high.position.y == doctest::Approx(0.0F));
    CHECK(FlatArea(mesh, 4 * kPlate) == doctest::Approx(2.0F));
}

TEST_CASE_FIXTURE(Parts, "faces whose command is exactly 1 are culled") {
    AddChassis(false);
    Add(test::WithBox(catalog.library, Block(0x900, 1, 1, 3), kCulledCommand));

    const CarMesh mesh = Mesh({{.x = 2, .y = 1}});

    CHECK(Triangles(mesh) == 10);
    CHECK(FlatArea(mesh, 4 * kPlate) == doctest::Approx(0.0F));
}

TEST_CASE_FIXTURE(Parts, "studs top every occupied cell in the piece's colour material") {
    AddChassis(false);
    AddStud(1);
    Add(test::WithBox(catalog.library, Block(0x900, 1, 1, 3), kNubCommand));

    const CarMesh mesh = Mesh({{.x = 2, .y = 1}});

    CHECK(mesh.studs == StudStyle::Studs);
    CHECK(Triangles(mesh) == 60 + 12);
    CHECK(mesh.geometry.materials == std::vector<std::string>{"red"});
}

void CheckNubTexture(const CarMesh& mesh) {
    REQUIRE(mesh.geometry.materials == std::vector<std::string>{"red", "redaa"});
    REQUIRE(mesh.geometry.ranges.size() == 2);
    CHECK(mesh.geometry.ranges[1].indexCount == 6);
    CHECK(std::ranges::any_of(mesh.geometry.vertices, [](const assets::GeometryVertex& vertex) {
        return vertex.u == doctest::Approx(0.25F) && vertex.v == doctest::Approx(0.25F);
    }));
}

TEST_CASE_FIXTURE(Parts, "studs that overflow give way to the textured nub material") {
    AddChassis(false);
    AddStud(50);
    Add(test::WithBox(catalog.library, Block(0x900, 1, 1, 3), kNubCommand));

    const CarMesh mesh = Mesh({{.x = 2, .y = 1}});

    CHECK(mesh.studs == StudStyle::NubTexture);
    CHECK(mesh.drawnPieces == 2);
    CHECK(Triangles(mesh) == 12);
    CheckNubTexture(mesh);
}

TEST_CASE_FIXTURE(Parts, "a piece that indexes past the library fails to build") {
    AddChassis(false);
    assets::PieceRecord broken = Block(0x900, 1, 1, 3);
    broken.indexOffset = static_cast<std::uint32_t>(catalog.library.indices.size());
    broken.commandCount = 1;
    catalog.library.indices.insert(catalog.library.indices.end(), {0, 999, 0, 0, 0});
    Add(broken);

    CHECK_FALSE(BuildCarMesh(Car({{.x = 2, .y = 1}}), catalog).has_value());
}

bool BuildsWhole(const assets::SavedRacer& racer, const assets::PieceCatalog& catalog,
                 const assets::PieceLibrary& menuLibrary) {
    const Result<BuiltCar> car = BuiltCar::Load(racer.car, menuLibrary);
    const Result<CarMesh> mesh =
        car.and_then([&](const BuiltCar& built) { return BuildCarMesh(built, catalog); });
    return mesh && mesh->studs == StudStyle::Studs && mesh->drawnPieces == car->Pieces().size() &&
           !mesh->geometry.indices.empty();
}

TEST_CASE("every retail car builds whole at both details") {
    const std::optional<test::RetailCars> menu = test::LoadRetailCars(assets::PieceDetail::Menu);
    const std::optional<test::RetailCars> race = test::LoadRetailCars(assets::PieceDetail::Race);
    if (!menu || !race) {
        return;
    }

    for (const test::RetailCars* cars : {&*menu, &*race}) {
        CHECK(std::ranges::all_of(cars->racers, [&](const assets::SavedRacer& racer) {
            return BuildsWhole(racer, cars->catalog, menu->catalog.library);
        }));
    }
}

}
}
