#include "assets/surface_table.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

std::vector<std::byte> Entry(std::string_view name, std::vector<std::byte> fields) {
    return test::Join({test::Bytes({0x27}), test::Text(name), test::Bytes({0x05}),
                       std::move(fields), test::Bytes({0x06})});
}

TEST_CASE("a surface table keeps only the values an entry sets") {
    const std::vector<std::byte> bytes = test::Join({
        test::Bytes({0x27}),
        test::ListHeader(2),
        Entry("water",
              test::Join({test::Bytes({0x36}), test::Floats({30}), test::Bytes({0x2D}),
                          test::Floats({50, 50, 0}), test::Bytes({0x31}), test::Text("wateremt")})),
        Entry("in", test::Join({test::Bytes({0x37, 0x38, 0x2A}), test::Ints({0})})),
        test::Bytes({0x06}),
    });

    const Result<std::vector<SurfaceEntry>> table = ParseSurfaceTable(bytes);

    REQUIRE(table.has_value());
    REQUIRE(table->size() == 2);
    const SurfaceEntry& water = (*table)[0];
    CHECK(water.name == "water");
    CHECK(water.rollingResistance == 30.0F);
    CHECK(water.force == Vector3{.x = 50.0F, .y = 50.0F});
    CHECK(water.particle == "wateremt");
    CHECK_FALSE(water.friction.has_value());
    CHECK_FALSE(water.isNonSolid);
    const SurfaceEntry& in = (*table)[1];
    CHECK(in.isNonSolid);
    CHECK(in.isProjectilePassable);
    CHECK(in.touchEvent == 0);
    CHECK_FALSE(in.isFinish);
}

TEST_CASE("every surface table in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    const test::Survey tables = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "TMB"; },
        [](std::span<const std::byte> bytes) {
            return ParseSurfaceTable(bytes).transform([](const auto&) {});
        });
    test::CheckSurvey(tables, 14);
}

}
}
