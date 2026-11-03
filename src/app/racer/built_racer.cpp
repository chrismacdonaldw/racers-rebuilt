#include "app/racer/built_racer.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "app/racer/racer_light.hpp"
#include "app/racer/racer_models.hpp"
#include "sim/animation/skeleton_pose.hpp"
#include "sim/physics/racer_chassis.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::string_view kPieceFolder = "MENUDATA/PIECEDB";
constexpr std::string_view kPartFolder = "MENUDATA/PARTDB";
constexpr std::string_view kCarModel = "BUILT_CAR";
constexpr std::string_view kDriverModel = "BUILT_DRIVER";
constexpr std::int32_t kBuiltStat = 100;

Result<void> LoadInto(assets::TrackLoader& loader, assets::TrackAssets& into,
                      std::string_view folder, const assets::ModelFiles& files) {
    return loader.LoadModels(folder, std::span(&files, 1))
        .transform(
            [&](assets::TrackAssets loaded) { assets::MergeTrackAssets(into, std::move(loaded)); });
}

Result<assets::TrackAssets> LoadModels(assets::TrackLoader& loader,
                                       const assets::ChassisEntry& chassis,
                                       const std::optional<assets::ChampionEntry>& champion) {
    assets::TrackAssets models;
    return LoadInto(loader, models, kCommonFolder, chassis.wheels.model)
        .and_then([&]() -> Result<void> {
            if (!champion) {
                return {};
            }
            return LoadInto(loader, models, kCommonFolder, champion->body);
        })
        .and_then([&] {
            return LoadInto(loader, models, kPieceFolder,
                            {.materials = "LPIECELO", .textures = "LPIECELO"});
        })
        .and_then([&] {
            return LoadInto(loader, models, kPartFolder,
                            {.materials = "BODYPART", .textures = "BODYPART"});
        })
        .transform([&] { return std::move(models); });
}

assets::RacerFiles BuiltFiles(const settings::Racer& racer, assets::ChassisEntry chassis,
                              const sim::CarBuild& build) {
    const sim::BodyConstants body = sim::MakeBodyConstants(sim::BuiltChassis(chassis), build);
    return {.driver = {.name = racer.name,
                       .nameIndex = -1,
                       .model = {.geometry = std::string(kDriverModel)},
                       .champion = std::string(kCarModel),
                       .redStat = kBuiltStat,
                       .yellowStat = kBuiltStat,
                       .greenStat = kBuiltStat,
                       .blueStat = kBuiltStat},
            .champion = {.name = std::string(kCarModel),
                         .body = {.geometry = std::string(kCarModel)},
                         .chassis = chassis.name,
                         .mass = body.mass,
                         .centreOfMass = body.centreOfMass},
            .chassis = std::move(chassis)};
}

void AddModel(assets::TrackAssets& models, std::string_view name, assets::GeometryAsset geometry) {
    models.models.insert_or_assign(
        std::string(name), std::make_shared<const assets::GeometryAsset>(std::move(geometry)));
}

}

Result<RacerAssets> LoadBuiltRacer(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                   const assets::RacerCatalog& catalog,
                                   const settings::Racer& racer, CarBody carBody) {
    const Result<assets::DriverPartCatalog> parts = LoadDriverParts(vfs);
    Result<assets::GeometryAsset> driver =
        parts.and_then([&](const assets::DriverPartCatalog& catalog) {
            return BuildMinifig(vfs, catalog, racer.look, assets::DriverDetail::Race);
        });
    Result<assets::ChassisEntry> chassis = driver.and_then(
        [&](const assets::GeometryAsset&) { return catalog.FindChassis(racer.chassis); });
    Result<BuiltCarModel> body = chassis.and_then([&](const assets::ChassisEntry&) {
        return BuildCarModel(vfs, racer.car, assets::PieceDetail::Race);
    });
    Result<std::optional<assets::ChampionEntry>> champion = body.and_then(
        [&](const BuiltCarModel&) { return ChampionBodyOf(vfs, catalog, racer, carBody); });
    Result<assets::TrackAssets> models =
        champion.and_then([&](const std::optional<assets::ChampionEntry>& found) {
            return LoadModels(loader, *chassis, found);
        });
    if (!models) {
        return std::unexpected(std::move(models.error()));
    }
    GreyMinifigMaterials(models->materials, *parts, racer.look);
    assets::RacerFiles files = BuiltFiles(racer, std::move(*chassis), body->build);
    if (*champion) {
        files.champion.body = (*champion)->body;
    } else {
        BakeBuiltRacerLight(body->geometry, models->materials, {});
        AddModel(*models, kCarModel, std::move(body->geometry));
    }
    return LoadRacerJoints(
               vfs, {.files = std::move(files), .models = std::move(*models), .build = body->build})
        .transform([&](RacerAssets loaded) {
            sim::SkeletonPose pose(loaded.driver.skeleton, 1.0F);
            BakeBuiltRacerLight(*driver, loaded.models.materials, pose.Solve());
            AddModel(loaded.models, kDriverModel, std::move(*driver));
            return loaded;
        });
}

}
