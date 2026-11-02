#include "app/racer/built_racer.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "app/racer/racer_models.hpp"
#include "sim/physics/racer_chassis.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::string_view kPieceFolder = "MENUDATA/PIECEDB";
constexpr std::string_view kPartFolder = "MENUDATA/PARTDB";
constexpr std::string_view kCarModel = "BUILT_CAR";
constexpr std::string_view kDriverModel = "BUILT_DRIVER";
constexpr std::int32_t kBuiltStat = 100;

void Merge(assets::TrackAssets& into, assets::TrackAssets from) {
    into.models.merge(from.models);
    into.materials.merge(from.materials);
    into.textures.merge(from.textures);
    std::ranges::move(from.missingTextures, std::back_inserter(into.missingTextures));
}

Result<void> LoadInto(assets::TrackLoader& loader, assets::TrackAssets& into,
                      std::string_view folder, const assets::ModelFiles& files) {
    return loader.LoadModels(folder, std::span(&files, 1))
        .transform([&](assets::TrackAssets loaded) { Merge(into, std::move(loaded)); });
}

Result<assets::TrackAssets> LoadModels(assets::TrackLoader& loader,
                                       const assets::ChassisEntry& chassis) {
    assets::TrackAssets models;
    return LoadInto(loader, models, kCommonFolder, chassis.wheels.model)
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
                                   const settings::Racer& racer) {
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
    Result<assets::TrackAssets> models =
        body.and_then([&](const BuiltCarModel&) { return LoadModels(loader, *chassis); });
    if (!models) {
        return std::unexpected(std::move(models.error()));
    }
    GreyMinifigMaterials(models->materials, *parts, racer.look);
    AddModel(*models, kCarModel, std::move(body->geometry));
    AddModel(*models, kDriverModel, std::move(*driver));
    return LoadRacerJoints(vfs, {.files = BuiltFiles(racer, std::move(*chassis), body->build),
                                 .models = std::move(*models),
                                 .build = body->build});
}

}
