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

}

Result<std::optional<JointedModel>> BuildStandCar(render::Renderer& renderer,
                                                  const assets::Vfs& vfs,
                                                  assets::TrackLoader& loader,
                                                  const settings::Racer& racer, CarBody carBody) {
    return LoadAwardCar(vfs, loader, racer, carBody)
        .and_then([&](const std::optional<AwardCar>& car) -> Result<std::optional<JointedModel>> {
            if (!car) {
                return std::nullopt;
            }
            return Upload(renderer, car->model, car->looks);
        });
}

Result<StandRacer> BuildStandDriver(render::Renderer& renderer, const assets::Vfs& vfs,
                                    assets::TrackLoader& loader, const assets::DriverLook& look) {
    Result<AwardDriver> driver = LoadAwardDriver(vfs, loader, look);
    Result<assets::SkeletonAsset> skeleton = driver.and_then([&](const AwardDriver&) {
        return LoadDriverParts(vfs).and_then([&](const assets::DriverPartCatalog& parts) {
            return LoadMenuMinifigSkeleton(vfs, parts, look);
        });
    });
    Result<JointedModel> minifig = skeleton.and_then([&](const assets::SkeletonAsset&) {
        return Upload(renderer, driver->minifig, driver->looks);
    });
    if (!minifig) {
        return std::unexpected(std::move(minifig.error()));
    }
    return StandRacer{.skeleton = std::move(*skeleton),
                      .scale = driver->minifig.scale,
                      .driver = std::move(*minifig)};
}

Result<StandRacer> BuildStandRacer(render::Renderer& renderer, const assets::Vfs& vfs,
                                   assets::TrackLoader& loader, const settings::Racer& racer,
                                   CarBody carBody) {
    return BuildStandDriver(renderer, vfs, loader, racer.look)
        .and_then([&](StandRacer stand) -> Result<StandRacer> {
            return BuildStandCar(renderer, vfs, loader, racer, carBody)
                .transform([&](std::optional<JointedModel> car) {
                    stand.car = std::move(car);
                    return std::move(stand);
                });
        });
}

}
