#include "app/racer/far_cars.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace rr::app {
namespace {

std::vector<std::uint32_t> IndexCounts(std::span<const render::MeshDraw> meshes) {
    std::vector<std::uint32_t> counts;
    for (const render::MeshDraw& mesh : meshes) {
        counts.push_back(mesh.indexCount);
    }
    return counts;
}

const std::array<render::MeshDraw, 2> kCarDraws{render::MeshDraw{.indexCount = 6},
                                                render::MeshDraw{.indexCount = 9}};

FarCars TwoCars() {
    FarCars farCars;
    farCars.SetHidden(true);
    farCars.Add({.position = {.x = 500.0F}, .firstDraw = 1, .draws = kCarDraws});
    farCars.Add({.position = {.x = 501.0F},
                 .firstDraw = 3,
                 .draws = std::span(kCarDraws).first(1),
                 .rangeSquared = 251001.0F});
    return farCars;
}

TEST_CASE("each view hides the cars past their range from its own camera") {
    FarCars farCars = TwoCars();
    std::vector<render::MeshDraw> meshes(4, render::MeshDraw{.indexCount = 3});

    const std::vector<std::uint32_t> second = IndexCounts(farCars.Cull(1, {.x = 1000.5F}, meshes));
    const std::vector<std::uint32_t> first = IndexCounts(farCars.Cull(0, {.x = 0.0F}, meshes));

    CHECK(first == std::vector<std::uint32_t>{3, 6, 9, 6});
    CHECK(second == std::vector<std::uint32_t>{3, 0, 0, 6});
    CHECK(IndexCounts(meshes) == first);
}

TEST_CASE("a later view gives back the counts an earlier view hid") {
    FarCars farCars = TwoCars();
    std::vector<render::MeshDraw> meshes(4, render::MeshDraw{.indexCount = 3});

    static_cast<void>(farCars.Cull(0, {.x = 2000.0F}, meshes));
    const std::vector<std::uint32_t> second = IndexCounts(farCars.Cull(1, {}, meshes));

    CHECK(second == std::vector<std::uint32_t>{3, 6, 9, 6});
}

TEST_CASE("far cars stay drawn unless hidden") {
    FarCars farCars;
    farCars.Add({.position = {.x = 9000.0F}, .firstDraw = 0, .draws = kCarDraws});
    std::vector<render::MeshDraw> meshes(2, render::MeshDraw{.indexCount = 3});

    CHECK(IndexCounts(farCars.Cull(0, {}, meshes)) == std::vector<std::uint32_t>{3, 3});
}

}
}
