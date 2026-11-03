#include "app/far_models.hpp"

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

std::vector<render::MeshDraw> Meshes() {
    return {render::MeshDraw{.indexCount = 3}, render::MeshDraw{.indexCount = 6},
            render::MeshDraw{.indexCount = 9}, render::MeshDraw{.indexCount = 0}};
}

FarModels CarAndScenery() {
    FarModels models(
        {{.position = {.x = 100.0F}, .draws = {.first = 0, .count = 1}, .rangeSquared = 10000.0F}});
    models.SetHidden(true, true);
    models.Add(
        FarKind::Car,
        {.position = {.x = 500.0F}, .draws = {.first = 1, .count = 3}, .rangeSquared = 250000.0F});
    return models;
}

TEST_CASE("each view hides the models past their range from its own camera") {
    FarModels models = CarAndScenery();
    std::vector<render::MeshDraw> meshes = Meshes();

    const std::vector<std::uint32_t> second = IndexCounts(models.Cull(1, {.x = 1000.5F}, meshes));
    const std::vector<std::uint32_t> first = IndexCounts(models.Cull(0, {.x = 200.5F}, meshes));

    CHECK(first == std::vector<std::uint32_t>{0, 6, 9, 0});
    CHECK(second == std::vector<std::uint32_t>{0, 0, 0, 0});
    CHECK(IndexCounts(meshes) == first);
}

TEST_CASE("a later view gives back the counts the first view hid") {
    FarModels models = CarAndScenery();
    std::vector<render::MeshDraw> meshes = Meshes();

    static_cast<void>(models.Cull(0, {.x = 2000.0F}, meshes));
    const std::vector<std::uint32_t> second = IndexCounts(models.Cull(1, {}, meshes));

    CHECK(second == std::vector<std::uint32_t>{3, 6, 9, 0});
}

TEST_CASE("reset gives back only what the first view hid and forgets the cars") {
    FarModels models = CarAndScenery();
    std::vector<render::MeshDraw> meshes = Meshes();
    static_cast<void>(models.Cull(0, {.x = 2000.0F}, meshes));

    models.Reset(meshes);
    CHECK(IndexCounts(meshes) == std::vector<std::uint32_t>{3, 6, 9, 0});

    const std::vector<std::uint32_t> culled = IndexCounts(models.Cull(0, {.x = 2000.0F}, meshes));
    CHECK(culled == std::vector<std::uint32_t>{0, 6, 9, 0});
}

TEST_CASE("far models stay drawn unless hidden") {
    FarModels models = CarAndScenery();
    models.SetHidden(false, false);
    std::vector<render::MeshDraw> meshes = Meshes();

    CHECK(IndexCounts(models.Cull(0, {.x = 9000.0F}, meshes)) ==
          std::vector<std::uint32_t>{3, 6, 9, 0});
}

TEST_CASE("cars are added only while far cars are hidden") {
    FarModels models;
    models.SetHidden(false, true);
    models.Add(FarKind::Car,
               {.position = {}, .draws = {.first = 0, .count = 1}, .rangeSquared = 1.0F});
    std::vector<render::MeshDraw> meshes = Meshes();

    CHECK(IndexCounts(models.Cull(0, {.x = 9000.0F}, meshes)) ==
          std::vector<std::uint32_t>{3, 6, 9, 0});
}

TEST_CASE("added models count their draws from where they start and last one frame") {
    FarModels models;
    models.SetHidden(true, true);
    const std::array<FarModel, 1> pickups{
        FarModel{.position = {}, .draws = {.first = 1, .count = 1}, .rangeSquared = 1.0F}};
    models.Add(FarKind::Scenery, pickups, 1);
    std::vector<render::MeshDraw> meshes = Meshes();

    CHECK(IndexCounts(models.Cull(0, {.x = 9000.0F}, meshes)) ==
          std::vector<std::uint32_t>{3, 6, 0, 0});

    models.Reset(meshes);
    CHECK(IndexCounts(models.Cull(0, {.x = 9000.0F}, meshes)) ==
          std::vector<std::uint32_t>{3, 6, 9, 0});
}

}
}
