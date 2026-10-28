#include <doctest/doctest.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "assets/geometry_database.hpp"
#include "assets/jam_archive.hpp"
#include "assets/material_library.hpp"
#include "assets/world_database.hpp"

namespace rr::assets {
namespace {

TEST_CASE("every model and material library in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

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
    test::CheckSurvey(models, 812);

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
    test::CheckSurvey(libraries, 208);
    CHECK(materialCount == 2420);
}

TEST_CASE("every world in the retail game parses") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    std::size_t entityCount = 0;
    const test::Survey worlds = test::SurveyLayer(
        *archive, [](std::string_view name) { return test::Extension(name) == "WDB"; },
        [&](std::span<const std::byte> bytes) {
            return ParseWorld(bytes).transform([&](const WorldAsset& world) {
                entityCount += world.modelEntities.size() + world.animatedEntities.size() +
                               world.collidableEntities.size() + world.boundedEntities.size();
            });
        });
    MESSAGE("WDB: ", worlds.fileCount, " files, ", entityCount, " entities");
    test::CheckSurvey(worlds, 133);
    CHECK(entityCount == 980);
}

}
}
