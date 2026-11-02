#include "app/cutscene/award_driver.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "app/racer/racer_models.hpp"
#include "assets/driver_parts.hpp"
#include "assets/game_path.hpp"
#include "assets/racer_catalog.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPartFolder = "MENUDATA/PARTDB";
constexpr std::string_view kDriverEntity = "GUY1";
constexpr std::array<std::string_view, 2> kLegVariants{"SWAP", "PLEG"};
constexpr std::size_t kLegsSlot = 1;

std::uint8_t PartIndex(std::int32_t index) {
    return static_cast<std::uint8_t>(index);
}

std::optional<std::string> ModelOf(const assets::TrackAssets& assets, std::string_view entity) {
    for (const assets::AssetCache<assets::WorldAsset>::Handle& world : assets.worlds) {
        for (const assets::WorldEntity& candidate : world->animatedEntities) {
            const bool hasModel =
                !candidate.lods.empty() && candidate.lods.front().model != assets::kNoIndex;
            if (hasModel && assets::ToUpperAscii(candidate.name) == entity) {
                return assets::ToUpperAscii(
                    world->models.at(static_cast<std::size_t>(candidate.lods.front().model)));
            }
        }
    }
    return std::nullopt;
}

void GiveLegs(assets::TrackAssets& assets, std::string_view entity, const std::string& legs) {
    const std::optional<std::string> name = ModelOf(assets, entity);
    const auto model = name ? assets.models.find(*name) : assets.models.end();
    if (model == assets.models.end() || model->second->materials.empty()) {
        return;
    }
    assets::GeometryAsset dressed = *model->second;
    dressed.materials.front() = legs;
    model->second = std::make_shared<const assets::GeometryAsset>(std::move(dressed));
}

}

assets::DriverLook DriverLookOf(const assets::DriverEntry& driver) {
    return {.face = PartIndex(driver.faceIndex),
            .hat = PartIndex(driver.hatIndex),
            .legs = PartIndex(driver.legsIndex),
            .torso = PartIndex(driver.torsoIndex),
            .expression = PartIndex(driver.expressionIndex)};
}

Result<assets::DriverLook> AwardLook(const assets::Vfs& vfs, const CutscenePlay& play) {
    if (play.playerRacer) {
        return play.playerRacer->look;
    }
    return assets::RacerCatalog::Load(vfs)
        .and_then([&](const assets::RacerCatalog& catalog) { return catalog.Find(play.driver); })
        .transform([](const assets::RacerFiles& racer) { return DriverLookOf(racer.driver); });
}

Result<AwardDriver> LoadAwardDriver(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                    const assets::DriverLook& look) {
    const assets::ModelFiles files{.materials = "BODYPART", .textures = "BODYPART"};
    const Result<assets::DriverPartCatalog> parts = LoadDriverParts(vfs);
    Result<assets::GeometryAsset> minifig =
        parts.and_then([&](const assets::DriverPartCatalog& catalog) {
            return BuildMinifig(vfs, catalog, look, assets::DriverDetail::Menu);
        });
    Result<assets::TrackAssets> looks = minifig.and_then([&](const assets::GeometryAsset&) {
        return loader.LoadModels(kPartFolder, std::span(&files, 1));
    });
    const std::string face = minifig ? parts->faces.parts.at(look.face).name : std::string();
    Result<assets::TrackAssets> faces = looks.and_then([&](const assets::TrackAssets& loaded) {
        return LoadExpressionFaces(vfs, loaded.materials, face);
    });
    if (!faces) {
        return std::unexpected(std::move(faces.error()));
    }
    assets::MergeTrackAssets(*looks, std::move(*faces));
    return AwardDriver{
        .look = look, .face = face, .minifig = std::move(*minifig), .looks = std::move(*looks)};
}

void DressAwardDriver(const AwardDriver& driver, assets::TrackAssets& assets) {
    if (const std::optional<std::string> model = ModelOf(assets, kDriverEntity)) {
        assets.models.insert_or_assign(
            *model, std::make_shared<const assets::GeometryAsset>(driver.minifig));
    }
    if (driver.minifig.materials.size() > kLegsSlot) {
        for (const std::string_view variant : kLegVariants) {
            GiveLegs(assets, variant, driver.minifig.materials[kLegsSlot]);
        }
    }
    for (const auto& [name, material] : driver.looks.materials) {
        assets.materials.insert_or_assign(name, material);
    }
    for (const auto& [name, texture] : driver.looks.textures) {
        assets.textures.insert_or_assign(name, texture);
    }
}

}
