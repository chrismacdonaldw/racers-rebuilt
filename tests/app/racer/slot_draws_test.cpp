#include "app/racer/slot_draws.hpp"

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

std::vector<render::MeshDraw> Meshes() {
    return {render::MeshDraw{.indexCount = 3}, render::MeshDraw{.indexCount = 6},
            render::MeshDraw{.indexCount = 9}, render::MeshDraw{.indexCount = 12}};
}

TEST_CASE("a hidden span draws nothing in the copy and the scene's meshes keep it") {
    const std::vector<render::MeshDraw> meshes = Meshes();
    std::vector<render::MeshDraw> copy;

    const std::span<const render::MeshDraw> shown =
        WithoutDraws(meshes, {.first = 1, .count = 2}, copy);

    CHECK(IndexCounts(shown) == std::vector<std::uint32_t>{3, 0, 0, 12});
    CHECK(IndexCounts(meshes) == std::vector<std::uint32_t>{3, 6, 9, 12});
    CHECK(shown.data() == copy.data());
}

TEST_CASE("a hidden span past the last mesh stops at the end") {
    std::vector<render::MeshDraw> copy;

    CHECK(IndexCounts(WithoutDraws(Meshes(), {.first = 3, .count = 4}, copy)) ==
          std::vector<std::uint32_t>{3, 6, 9, 0});
    CHECK(IndexCounts(WithoutDraws(Meshes(), {.first = 8, .count = 1}, copy)) ==
          std::vector<std::uint32_t>{3, 6, 9, 12});
}

}
}
