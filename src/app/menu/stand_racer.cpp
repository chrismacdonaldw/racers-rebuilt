#include "app/menu/stand_racer.hpp"

#include <utility>

#include "app/cutscene/award_car.hpp"
#include "app/cutscene/award_driver.hpp"
#include "app/racer/racer_models.hpp"

namespace rr::app {
namespace {

Result<JointedModel> Upload(render::Renderer& renderer, const assets::GeometryAsset& model,
                            const assets::TrackAssets& looks) {
    return UploadMaterials(renderer, looks, model.materials)
        .and_then([&](const assets::NameMap<render::Material>& materials) {
            return UploadJointedModel(renderer, model, materials);
        });
}

Result<std::optional<JointedModel>> UploadCar(render::Renderer& renderer, const assets::Vfs& vfs,
                                              assets::TrackLoader& loader,
                                              const settings::Racer& racer) {
    return LoadAwardCar(vfs, loader, racer)
        .and_then([&](const std::optional<AwardCar>& car) -> Result<std::optional<JointedModel>> {
            if (!car) {
                return std::nullopt;
            }
            return Upload(renderer, car->model, car->looks);
        });
}

}

Result<StandRacer> BuildStandRacer(render::Renderer& renderer, const assets::Vfs& vfs,
                                   assets::TrackLoader& loader, const settings::Racer& racer) {
    Result<AwardDriver> driver = LoadAwardDriver(vfs, loader, racer.look);
    Result<assets::SkeletonAsset> skeleton = driver.and_then([&](const AwardDriver&) {
        return LoadDriverParts(vfs).and_then([&](const assets::DriverPartCatalog& parts) {
            return LoadMenuMinifigSkeleton(vfs, parts, racer.look);
        });
    });
    Result<JointedModel> minifig = skeleton.and_then([&](const assets::SkeletonAsset&) {
        return Upload(renderer, driver->minifig, driver->looks);
    });
    Result<std::optional<JointedModel>> car = minifig.and_then(
        [&](const JointedModel&) { return UploadCar(renderer, vfs, loader, racer); });
    if (!car) {
        return std::unexpected(std::move(car.error()));
    }
    return StandRacer{.skeleton = std::move(*skeleton),
                      .scale = driver->minifig.scale,
                      .driver = std::move(*minifig),
                      .car = std::move(*car)};
}

}
