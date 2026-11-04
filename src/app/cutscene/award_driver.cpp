#include "app/cutscene/award_driver.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "app/cutscene/cutscene_racer.hpp"
#include "app/racer/racer_models.hpp"
#include "assets/driver_model.hpp"
#include "assets/driver_parts.hpp"
#include "assets/game_path.hpp"
#include "assets/racer_catalog.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kDriverEntity = "GUY1";
constexpr std::array<std::string_view, 2> kLegVariants{"SWAP", "PLEG"};
constexpr std::size_t kLegsSlot = 1;

bool IsPartIndex(std::int32_t index) {
    return std::in_range<std::uint8_t>(index);
}

void GiveLegs(assets::TrackAssets& assets, std::string_view entity, const std::string& legs) {
    const std::optional<std::string> name = EntityModel(assets, entity);
    const auto model = name ? assets.models.find(*name) : assets.models.end();
    if (model == assets.models.end() || model->second->materials.empty()) {
        return;
    }
    assets::GeometryAsset dressed = *model->second;
    dressed.materials.front() = legs;
    model->second = std::make_shared<const assets::GeometryAsset>(std::move(dressed));
}

}

Result<assets::DriverLook> DriverLookOf(const assets::DriverEntry& driver) {
    const std::array indices{driver.faceIndex, driver.hatIndex, driver.legsIndex, driver.torsoIndex,
                             driver.expressionIndex};
    if (!std::ranges::all_of(indices, IsPartIndex)) {
        return Fail(ErrorCode::InvalidData,
                    std::format("driver {} has a part index out of range", driver.name));
    }
    return assets::DriverLook{.face = static_cast<std::uint8_t>(driver.faceIndex),
                              .hat = static_cast<std::uint8_t>(driver.hatIndex),
                              .legs = static_cast<std::uint8_t>(driver.legsIndex),
                              .torso = static_cast<std::uint8_t>(driver.torsoIndex),
                              .expression = static_cast<std::uint8_t>(driver.expressionIndex)};
}

Result<assets::DriverLook> AwardLook(const assets::Vfs& vfs, const CutscenePlay& play) {
    if (play.playerRacer) {
        return play.playerRacer->look;
    }
    return assets::RacerCatalog::Load(vfs)
        .and_then([&](const assets::RacerCatalog& catalog) { return catalog.Find(play.driver); })
        .and_then([](const assets::RacerFiles& racer) { return DriverLookOf(racer.driver); });
}

Result<AwardDriver> LoadAwardDriver(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                    const assets::DriverLook& look) {
    const Result<assets::DriverPartCatalog> parts = LoadDriverParts(vfs);
    Result<assets::GeometryAsset> minifig =
        parts.and_then([&](const assets::DriverPartCatalog& catalog) {
            return BuildMinifig(vfs, catalog, look, assets::DriverDetail::Menu);
        });
    Result<assets::TrackAssets> looks = minifig.and_then(
        [&](const assets::GeometryAsset&) { return LoadDriverLooks(vfs, loader, *parts); });
    const std::string face =
        minifig ? parts->faces.parts.at(assets::FitLook(*parts, look).face).name : std::string();
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

std::optional<std::string> DriverModelOf(const assets::TrackAssets& assets) {
    return EntityModel(assets, kDriverEntity);
}

void DressAwardDriver(const AwardDriver& driver, assets::TrackAssets& assets) {
    if (const std::optional<std::string> model = DriverModelOf(assets)) {
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
