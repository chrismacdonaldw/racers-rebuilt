#include "app/racer/racer_bounds.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

#include "assets/archive_survey.hpp"
#include "assets/loader_pool.hpp"

namespace rr::app {
namespace {

assets::GeometryVertex At(const Vector3& position) {
    return {.position = position};
}

TEST_CASE("model bounds centre on the box around the vertices and reach the farthest one") {
    const std::vector<assets::GeometryVertex> vertices{At({.x = -1.0F}), At({.x = 3.0F}),
                                                       At({.y = 1.0F}), At({.z = -2.0F})};

    const BoundingSphere bounds = ModelBounds(vertices);

    CHECK(bounds.centre == Vector3{.x = 1.0F, .y = 0.5F, .z = -1.0F});
    CHECK(bounds.radius == doctest::Approx(2.2912878F));
}

TEST_CASE("group bounds centre on the mean member centre and cover every member") {
    const std::array members{BoundingSphere{.centre = {.x = -2.0F}, .radius = 1.0F},
                             BoundingSphere{.centre = {.x = 4.0F}, .radius = 3.0F}};

    const BoundingSphere group = GroupBounds(members);

    CHECK(group.centre == Vector3{.x = 1.0F});
    CHECK(group.radius == 6.0F);
}

constexpr float kTableRounding = 0.011F;

struct RetailBounds {
    std::string_view code;
    Vector3 centre;
    float radius = 0.0F;
};

constexpr std::array kRetailBounds{
    RetailBounds{.code = "CC", .centre = {.x = -0.86F, .y = 0.01F, .z = 2.03F}, .radius = 6.64F},
    RetailBounds{.code = "KK", .centre = {.x = -0.72F, .y = 0.00F, .z = 3.42F}, .radius = 7.58F},
    RetailBounds{.code = "JT", .centre = {.x = -0.12F, .y = 0.36F, .z = 2.59F}, .radius = 6.87F},
    RetailBounds{.code = "RR", .centre = {.x = 0.53F, .y = 0.01F, .z = 2.37F}, .radius = 7.95F},
    RetailBounds{.code = "BVB", .centre = {.x = 0.71F, .y = 0.34F, .z = 3.66F}, .radius = 8.78F},
};

void CheckRacer(const assets::Vfs& vfs, assets::TrackLoader& loader,
                const assets::RacerCatalog& catalog, const RetailBounds& expected) {
    CAPTURE(expected.code);
    const Result<BoundingSphere> bounds =
        LoadRacer(vfs, loader, catalog, expected.code).and_then(RacerBounds);
    REQUIRE(bounds.has_value());
    CHECK(Length(bounds->centre - expected.centre) < kTableRounding);
    CHECK(std::abs(bounds->radius - expected.radius) < kTableRounding);
}

TEST_CASE("prebuilt racers' bounds match the values worked out from the retail files") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    const Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(*vfs);
    REQUIRE(catalog.has_value());

    for (const RetailBounds& expected : kRetailBounds) {
        CheckRacer(*vfs, loader, *catalog, expected);
    }
}

}
}
