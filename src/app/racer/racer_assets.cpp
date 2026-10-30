#include "app/racer/racer_assets.hpp"

#include <array>
#include <format>
#include <string>
#include <utility>

namespace rr::app {
namespace {

constexpr std::string_view kCommonFolder = "GAMEDATA/COMMON";
constexpr std::string_view kDriverSkeleton = "PELVIS";

template <typename Asset>
Result<Asset> ReadCommon(const assets::Vfs& vfs, std::string_view name, std::string_view extension,
                         Result<Asset> (*parse)(std::span<const std::byte>)) {
    return vfs.Read(std::format("{}/{}.{}", kCommonFolder, name, extension))
        .and_then([parse](const assets::FileData& data) { return parse(data.Bytes()); });
}

Result<JointedAssets> LoadJointed(const assets::Vfs& vfs, std::string_view skeleton,
                                  std::string_view animations) {
    JointedAssets jointed;
    return ReadCommon(vfs, skeleton, "SDB", assets::ParseSkeleton)
        .and_then([&](assets::SkeletonAsset parsed) {
            jointed.skeleton = std::move(parsed);
            return ReadCommon(vfs, animations, "ADB", assets::ParseAnimations);
        })
        .transform([&](assets::AnimationAsset parsed) {
            jointed.animations = std::move(parsed);
            return std::move(jointed);
        });
}

}

Result<RacerAssets> LoadRacer(const assets::Vfs& vfs, assets::TrackLoader& loader,
                              const assets::RacerCatalog& catalog, std::string_view code) {
    RacerAssets racer;
    return catalog.Find(code)
        .and_then([&](assets::RacerFiles files) {
            racer.files = std::move(files);
            const std::array models{racer.files.chassis.wheels.model, racer.files.champion.body,
                                    racer.files.driver.model};
            return loader.LoadModels(kCommonFolder, models);
        })
        .and_then([&](assets::TrackAssets models) {
            racer.models = std::move(models);
            const assets::JointedModelFiles& wheels = racer.files.chassis.wheels;
            return LoadJointed(vfs, wheels.skeleton, wheels.animations);
        })
        .and_then([&](JointedAssets wheels) {
            racer.wheels = std::move(wheels);
            return LoadJointed(vfs, kDriverSkeleton, kDriverSkeleton);
        })
        .transform([&](JointedAssets driver) {
            racer.driver = std::move(driver);
            return std::move(racer);
        });
}

}
