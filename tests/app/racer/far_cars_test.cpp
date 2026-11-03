#include "app/racer/far_cars.hpp"

#include <doctest/doctest.h>

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

TEST_CASE("cars past 500 from a view's camera are hidden from that view only") {
    FarCars farCars({2, 1});
    farCars.SetHidden(true);
    const std::vector<render::MeshDraw> meshes(4, render::MeshDraw{.indexCount = 6});
    const std::vector<sim::EntitySnapshot> poses{{.transform = {.position = {.x = 500.0F}}},
                                                 {.transform = {.position = {.x = 501.0F}}}};

    const std::vector<std::uint32_t> first = IndexCounts(farCars.Cull(0, {}, poses, meshes, 1));
    const std::vector<std::uint32_t> second =
        IndexCounts(farCars.Cull(1, {.x = 1000.5F}, poses, meshes, 1));

    CHECK(first == std::vector<std::uint32_t>{6, 6, 6, 0});
    CHECK(second == std::vector<std::uint32_t>{6, 0, 0, 6});
}

TEST_CASE("far cars stay drawn unless hidden") {
    FarCars farCars({1});
    const std::vector<render::MeshDraw> meshes(1, render::MeshDraw{.indexCount = 6});
    const std::vector<sim::EntitySnapshot> poses{{.transform = {.position = {.x = 9000.0F}}}};

    CHECK(IndexCounts(farCars.Cull(0, {}, poses, meshes, 0)) == std::vector<std::uint32_t>{6});
}

}
}
