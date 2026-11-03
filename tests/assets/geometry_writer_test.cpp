#include "assets/geometry_writer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "assets/geometry_database.hpp"
#include "assets/jam_archive.hpp"

namespace rr::assets {
namespace {

constexpr std::size_t kGridSide = 12;
constexpr std::size_t kRetailModels = 812;

constexpr float kScaleTolerance = 1e-5F;

bool IsNear(float a, float b) {
    return std::abs(a - b) <= kScaleTolerance * std::max(1.0F, std::abs(a));
}

bool IsNear(const Vector3& a, const Vector3& b) {
    return IsNear(a.x, b.x) && IsNear(a.y, b.y) && IsNear(a.z, b.z);
}

struct Corner {
    std::string material;
    GeometryVertex vertex;

    friend bool operator==(const Corner& a, const Corner& b) {
        return a.material == b.material && IsNear(a.vertex.position, b.vertex.position) &&
               a.vertex.normal == b.vertex.normal && a.vertex.u == b.vertex.u &&
               a.vertex.v == b.vertex.v && a.vertex.rgba == b.vertex.rgba &&
               a.vertex.joint == b.vertex.joint;
    }
};

std::vector<Corner> Corners(const GeometryAsset& geometry) {
    std::vector<Corner> corners;
    for (const GeometryRange& range : geometry.ranges) {
        for (const std::uint32_t index :
             std::span(geometry.indices).subspan(range.firstIndex, range.indexCount)) {
            corners.push_back({.material = geometry.materials.at(range.material),
                               .vertex = geometry.vertices.at(index)});
        }
    }
    return corners;
}

GdbVertices LayoutOf(const GeometryAsset& geometry) {
    const bool hasNormals = std::ranges::any_of(
        geometry.vertices, [](const GeometryVertex& v) { return v.normal != Vector3{}; });
    return hasNormals ? GdbVertices::Lit : GdbVertices::Colored;
}

Result<std::vector<Corner>> RoundTrip(const GeometryAsset& geometry, GdbVertices layout) {
    return WriteGeometry(geometry, layout)
        .and_then([](const std::vector<std::byte>& bytes) { return ParseGeometry(bytes); })
        .transform(Corners);
}

GeometryVertex GridVertex(std::size_t x, std::size_t y) {
    const auto fx = static_cast<float>(x);
    const auto fy = static_cast<float>(y);
    return {.position = {.x = fx, .y = fy, .z = fx * fy},
            .normal = {.x = 0.0F, .y = 0.0F, .z = 1.0F},
            .u = fx / kGridSide,
            .v = fy / kGridSide,
            .joint = static_cast<std::uint16_t>(x < kGridSide / 2 ? 0 : 3)};
}

GeometryAsset Grid() {
    GeometryAsset grid{.materials = {"STONE", "GRASS"}};
    for (std::size_t y = 0; y < kGridSide; ++y) {
        for (std::size_t x = 0; x < kGridSide; ++x) {
            grid.vertices.push_back(GridVertex(x, y));
        }
    }
    const auto at = [](std::size_t x, std::size_t y) {
        return static_cast<std::uint32_t>((y * kGridSide) + x);
    };
    for (std::size_t y = 0; y + 1 < kGridSide; ++y) {
        const auto first = static_cast<std::uint32_t>(grid.indices.size());
        for (std::size_t x = 0; x + 1 < kGridSide; ++x) {
            grid.indices.insert(grid.indices.end(), {at(x, y), at(x + 1, y), at(x, y + 1),
                                                     at(x + 1, y), at(x + 1, y + 1), at(x, y + 1)});
        }
        grid.ranges.push_back(
            {.firstIndex = first,
             .indexCount = static_cast<std::uint32_t>(grid.indices.size()) - first,
             .material = static_cast<std::uint32_t>(y % 2)});
    }
    return grid;
}

TEST_CASE("a written model reads back with the same corners, past one vertex cache load") {
    const GeometryAsset grid = Grid();

    const Result<std::vector<Corner>> lit = RoundTrip(grid, GdbVertices::Lit);

    REQUIRE(lit.has_value());
    CHECK(lit.value_or(std::vector<Corner>{}).size() == grid.indices.size());
    CHECK(lit.value_or(std::vector<Corner>{}) == Corners(grid));
}

TEST_CASE("colored vertices keep their colours instead of normals") {
    GeometryAsset grid = Grid();
    for (GeometryVertex& vertex : grid.vertices) {
        vertex.normal = {};
        vertex.rgba = {static_cast<std::uint8_t>(vertex.position.x),
                       static_cast<std::uint8_t>(vertex.position.y), 9, 255};
    }

    CHECK(RoundTrip(grid, GdbVertices::Colored).value_or(std::vector<Corner>{}) == Corners(grid));
}

TEST_CASE("a scaled model keeps its scale and scaled positions") {
    GeometryAsset grid = Grid();
    grid.scale = 1.0F / 64.0F;

    const Result<GeometryAsset> back =
        WriteGeometry(grid, GdbVertices::Lit).and_then([](const std::vector<std::byte>& bytes) {
            return ParseGeometry(bytes);
        });

    REQUIRE(back.has_value());
    CHECK(back->scale == grid.scale);
    CHECK(Corners(*back) == Corners(grid));
}

TEST_CASE("a model can't be written with a material name the reader can't read") {
    GeometryAsset grid = Grid();
    grid.materials.front() = std::string(64, 'A');

    CHECK_FALSE(WriteGeometry(grid, GdbVertices::Lit).has_value());
}

TEST_CASE("every retail model writes and reads back with the same corners") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    const test::Survey models = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "GDB"; },
        [](std::span<const std::byte> bytes) {
            return ParseGeometry(bytes).and_then([](const GeometryAsset& geometry) -> Result<void> {
                const Result<std::vector<Corner>> back = RoundTrip(geometry, LayoutOf(geometry));
                if (!back) {
                    return std::unexpected(back.error());
                }
                if (*back != Corners(geometry)) {
                    return Fail(ErrorCode::InvalidData, "the corners changed");
                }
                return {};
            });
        });

    test::CheckSurvey(models, kRetailModels);
}

}
}
