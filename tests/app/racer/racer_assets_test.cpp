#include "app/racer/racer_assets.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "app/racer/racer_draws.hpp"
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

}
}
