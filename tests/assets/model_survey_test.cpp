#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "assets/geometry_database.hpp"
#include "assets/jam_archive.hpp"
#include "assets/material_library.hpp"
#include "game_files.hpp"

namespace rr::assets {
namespace {

void CheckSurvey(const test::Survey& survey, std::size_t expectedFiles) {
    CHECK(survey.fileCount == expectedFiles);
    for (const std::string& failure : survey.failures) {
        FAIL_CHECK(failure);
    }
}

TEST_CASE("every model and material library in the retail game parses") {
    const std::optional<std::filesystem::path> gameFolder = test::GameFolder();
    if (!gameFolder) {
        MESSAGE("RR_GAME_PATH isn't set, skipping");
        return;
    }
    const Result<JamArchive> archive = JamArchive::Open(*gameFolder / "LEGO.JAM");
    REQUIRE(archive.has_value());

    std::size_t vertexCount = 0;
    std::size_t indexCount = 0;
    const test::Survey models = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "GDB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseGeometry(bytes).transform([&](const GeometryAsset& geometry) {
                vertexCount += geometry.vertices.size();
                indexCount += geometry.indices.size();
            });
        });
    MESSAGE("GDB: ", models.fileCount, " files, ", vertexCount, " vertices, ", indexCount,
            " indices");
    CheckSurvey(models, 812);

    std::size_t materialCount = 0;
    const test::Survey libraries = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "MDB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseMaterialLibrary(bytes).transform(
                [&](const std::vector<MaterialAsset>& materials) {
                    materialCount += materials.size();
                });
        });
    MESSAGE("MDB: ", libraries.fileCount, " files, ", materialCount, " materials");
    CheckSurvey(libraries, 208);
    CHECK(materialCount == 2420);
}

}
}
