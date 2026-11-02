#include "app/racer/racer_assets.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/racer/built_racer.hpp"
#include "app/racer/racer_draws.hpp"
#include "app/racer/racer_lookup.hpp"
#include "app/racer/racer_models.hpp"
#include "app/racer/racer_visuals.hpp"
#include "assets/archive_survey.hpp"
#include "assets/game_path.hpp"
#include "assets/loader_pool.hpp"

namespace rr::app {
namespace {

Result<ModelDraws> StandInDraws(const RacerAssets& racer, std::string_view geometry) {
    const auto model = racer.models.models.find(assets::ToUpperAscii(geometry));
    if (model == racer.models.models.end()) {
        return Fail(ErrorCode::NotFound, std::format("{} wasn't loaded", geometry));
    }
    return FollowJoints(*model->second,
                        std::vector<render::MeshDraw>(model->second->ranges.size()));
}

bool IsFinite(const Transform& transform) {
    return std::isfinite(transform.position.x) && std::isfinite(transform.position.y) &&
           std::isfinite(transform.position.z) && std::isfinite(transform.rotation.w);
}

Result<void> PoseOnce(const RacerAssets& racer) {
    RacerDraws draws;
    Result<RacerVisuals> visuals =
        StandInDraws(racer, racer.files.chassis.wheels.model.geometry)
            .and_then([&](ModelDraws wheels) {
                draws.wheels = std::move(wheels);
                return StandInDraws(racer, racer.files.champion.body.geometry);
            })
            .and_then([&](ModelDraws body) {
                draws.body = std::move(body);
                return StandInDraws(racer, racer.files.driver.model.geometry);
            })
            .and_then([&](ModelDraws driver) {
                draws.driver = std::move(driver);
                return RacerVisuals::Create(racer, std::move(draws));
            });
    if (!visuals) {
        return std::unexpected(std::move(visuals.error()));
    }
    visuals->Update({}, {.id = {}, .forwardSpeed = 0.05F, .turnRadius = 20.0F}, 16.0F);
    for (const render::MeshDraw& draw : visuals->Draws()) {
        if (!IsFinite(draw.transform)) {
            return Fail(ErrorCode::InvalidData, "a draw was placed at a non-finite pose");
        }
    }
    return {};
}

TEST_CASE("every prebuilt racer's car and driver load and pose") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    const Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(*vfs);
    REQUIRE(catalog.has_value());

    test::Survey racers;
    for (const assets::DriverEntry& driver : catalog->Drivers()) {
        ++racers.fileCount;
        const Result<void> posed =
            LoadRacer(*vfs, loader, *catalog, driver.name).and_then([](const RacerAssets& racer) {
                return PoseOnce(racer);
            });
        if (!posed) {
            racers.failures.push_back(std::format("{}: {}", driver.name, posed.error().message));
        }
    }

    test::CheckSurvey(racers, 24);
}

std::vector<assets::SavedRacer> GameRacers(const assets::Vfs& vfs) {
    std::vector<assets::SavedRacer> racers;
    for (const char* file : {"MENUDATA/DEFAULT.LRS", "MENUDATA/QBUILD.LRS"}) {
        const Result<std::vector<assets::SavedRacer>> saved = vfs.Read(file).and_then(
            [](const assets::FileData& data) { return assets::ParseRacerFile(data.Bytes()); });
        REQUIRE(saved.has_value());
        racers.insert(racers.end(), saved->begin(), saved->end());
    }
    return racers;
}

TEST_CASE("every game racer's built car and minifig load and pose") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    const Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(*vfs);
    REQUIRE(catalog.has_value());

    test::Survey racers;
    for (const assets::SavedRacer& racer : GameRacers(*vfs)) {
        ++racers.fileCount;
        const Result<void> posed =
            LoadBuiltRacer(*vfs, loader, *catalog, RacerFromSave(racer))
                .and_then([](const RacerAssets& built) { return PoseOnce(built); });
        if (!posed) {
            racers.failures.push_back(posed.error().message);
        }
    }

    test::CheckSurvey(racers, 28);
}

Result<RacerAssets> LoadJoanOfKart(const assets::Vfs& vfs, assets::TrackLoader& loader) {
    const Result<assets::RacerCatalog> catalog = assets::RacerCatalog::Load(vfs);
    return catalog
        .and_then([&](const assets::RacerCatalog&) { return FindGameRacer(vfs, "JOAN OF KART"); })
        .and_then([&](const settings::Racer& racer) {
            return LoadBuiltRacer(vfs, loader, *catalog, racer);
        });
}

const assets::MaterialAsset& DefaultFace(const RacerAssets& racer) {
    const assets::GeometryAsset& driver =
        *racer.models.models.at(assets::ToUpperAscii(racer.files.driver.model.geometry));
    const auto face = std::ranges::find_if(
        driver.materials, [](const std::string& material) { return material.ends_with("dflt"); });
    REQUIRE(face != driver.materials.end());
    return racer.models.materials.at(assets::ToUpperAscii(*face));
}

TEST_CASE("a built racer weighs its bricks and its minifig's parts are grey") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    const Result<RacerAssets> joan = LoadJoanOfKart(*vfs, loader);
    REQUIRE(joan.has_value());

    CHECK(joan->build.value_or(sim::CarBuild{}).highPieceCount == 214);
    CHECK(joan->files.champion.mass == doctest::Approx(joan->files.chassis.baseMass + 214.0F));
    const assets::MaterialAsset& face = DefaultFace(*joan);
    CHECK(std::array{face.ambient[0], face.diffuse[2]} == std::array<std::uint8_t, 2>{200, 200});
}

TEST_CASE("a game racer's car and minifig also build at menu detail") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<settings::Racer> joan = FindGameRacer(*vfs, "JOAN OF KART");
    const Result<assets::DriverPartCatalog> parts = LoadDriverParts(*vfs);
    REQUIRE(joan.has_value());
    REQUIRE(parts.has_value());

    CHECK(BuildMinifig(*vfs, *parts, joan->look, assets::DriverDetail::Menu).has_value());
    CHECK(BuildCarModel(*vfs, joan->car, assets::PieceDetail::Menu).has_value());
}

}
}
