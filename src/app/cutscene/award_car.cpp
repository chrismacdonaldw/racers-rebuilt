#include "app/cutscene/award_car.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/cutscene/cutscene_racer.hpp"
#include "app/racer/racer_lookup.hpp"
#include "app/racer/racer_models.hpp"
#include "assets/chassis_database.hpp"
#include "assets/game_path.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/skeleton_database.hpp"
#include "core/transform.hpp"
#include "sim/animation/skeleton_pose.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kPieceFolder = "MENUDATA/PIECEDB";
constexpr std::string_view kCarBody = "CARBODY";

template <typename T>
Result<T> ReadParsed(const assets::Vfs& vfs, const std::string& path,
                     Result<T> (*parse)(std::span<const std::byte>)) {
    return vfs.Read(path).and_then(
        [parse](const assets::FileData& data) { return parse(data.Bytes()); });
}

Result<assets::ChassisEntry> FindMenuChassis(const assets::Vfs& vfs, std::string_view name) {
    return ReadParsed(vfs, std::format("{}/CHASSIS.CMB", kPieceFolder), assets::ParseChassis)
        .and_then(
            [&](const std::vector<assets::ChassisEntry>& chassis) -> Result<assets::ChassisEntry> {
                const std::string upper = assets::ToUpperAscii(name);
                const auto found =
                    std::ranges::find_if(chassis, [&](const assets::ChassisEntry& entry) {
                        return assets::ToUpperAscii(entry.name) == upper;
                    });
                if (found == chassis.end()) {
                    return Fail(ErrorCode::NotFound,
                                std::format("the menu has no chassis {}", name));
                }
                return *found;
            });
}

assets::GeometryAsset AtRest(const assets::GeometryAsset& model,
                             const assets::SkeletonAsset& skeleton) {
    sim::SkeletonPose pose(skeleton, model.scale);
    const std::span<const Transform> nodes = pose.Solve();
    assets::GeometryAsset rest = model;
    for (assets::GeometryVertex& vertex : rest.vertices) {
        const Transform node = vertex.joint < nodes.size() ? nodes[vertex.joint] : Transform{};
        vertex.position = Apply(node, vertex.position);
        vertex.normal = Rotate(node.rotation, vertex.normal);
        vertex.joint = 0;
    }
    for (assets::GeometryRange& range : rest.ranges) {
        range.joint = 0;
    }
    return rest;
}

void Append(assets::GeometryAsset& into, const assets::GeometryAsset& from) {
    const auto firstVertex = static_cast<std::uint32_t>(into.vertices.size());
    const auto firstIndex = static_cast<std::uint32_t>(into.indices.size());
    into.vertices.insert(into.vertices.end(), from.vertices.begin(), from.vertices.end());
    for (const std::uint32_t index : from.indices) {
        into.indices.push_back(firstVertex + index);
    }
    for (const assets::GeometryRange& range : from.ranges) {
        into.ranges.push_back({.firstIndex = firstIndex + range.firstIndex,
                               .indexCount = range.indexCount,
                               .material = assets::FindOrAddMaterial(
                                   into.materials, from.materials.at(range.material)),
                               .joint = range.joint});
    }
}

// NOLINTNEXTLINE(bugprone-exception-escape)
struct Chassis {
    assets::GeometryAsset model;
    assets::TrackAssets looks;
};

Result<Chassis> LoadChassis(const assets::Vfs& vfs, assets::TrackLoader& loader,
                            std::string_view name) {
    const Result<assets::ChassisEntry> entry = FindMenuChassis(vfs, name);
    const Result<assets::SkeletonAsset> skeleton =
        entry.and_then([&](const assets::ChassisEntry& found) {
            return ReadParsed(
                vfs,
                std::format("{}/{}.SDB", kPieceFolder, assets::ToUpperAscii(found.wheels.skeleton)),
                assets::ParseSkeleton);
        });
    Result<assets::TrackAssets> looks = skeleton.and_then([&](const assets::SkeletonAsset&) {
        return loader.LoadModels(kPieceFolder, std::span(&entry->wheels.model, 1));
    });
    if (!looks) {
        return std::unexpected(std::move(looks.error()));
    }
    const auto model = looks->models.find(assets::ToUpperAscii(entry->wheels.model.geometry));
    if (model == looks->models.end()) {
        return Fail(ErrorCode::NotFound,
                    std::format("chassis {} has no model {}", name, entry->wheels.model.geometry));
    }
    assets::GeometryAsset rest = AtRest(*model->second, *skeleton);
    looks->models.clear();
    return Chassis{.model = std::move(rest), .looks = std::move(*looks)};
}

}

Result<std::optional<settings::Racer>> AwardCarRacer(const assets::Vfs& vfs,
                                                     const CutscenePlay& play) {
    if (!play.showsChampionCar) {
        return play.playerRacer;
    }
    return assets::RacerCatalog::Load(vfs)
        .and_then([&](const assets::RacerCatalog& catalog) { return catalog.Find(play.driver); })
        .and_then([&](const assets::RacerFiles& champion) {
            return FindChampionCar(vfs, champion.chassis.name);
        })
        .transform([](settings::Racer racer) { return std::optional(std::move(racer)); });
}

Result<std::optional<AwardCar>> LoadAwardCar(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                             const settings::Racer& racer) {
    const assets::ModelFiles pieces{.materials = "LPIECEHI", .textures = "LPIECEHI"};
    Result<BuiltCarModel> body = BuildCarModel(vfs, racer.car, assets::PieceDetail::Menu);
    if (body && body->geometry.indices.empty()) {
        return std::nullopt;
    }
    Result<Chassis> chassis = body.and_then(
        [&](const BuiltCarModel&) { return LoadChassis(vfs, loader, racer.chassis); });
    Result<assets::TrackAssets> looks = chassis.and_then(
        [&](const Chassis&) { return loader.LoadModels(kPieceFolder, std::span(&pieces, 1)); });
    if (!looks) {
        return std::unexpected(std::move(looks.error()));
    }
    assets::MergeTrackAssets(*looks, std::move(chassis->looks));
    Append(body->geometry, chassis->model);
    return AwardCar{.model = std::move(body->geometry), .looks = std::move(*looks)};
}

bool HasCarBody(const assets::TrackAssets& assets) {
    return EntityModel(assets, kCarBody).has_value();
}

void DressAwardCar(const AwardCar& car, assets::TrackAssets& assets) {
    const std::optional<std::string> model = EntityModel(assets, kCarBody);
    if (!model) {
        return;
    }
    assets.models.insert_or_assign(*model,
                                   std::make_shared<const assets::GeometryAsset>(car.model));
    for (const auto& [name, material] : car.looks.materials) {
        assets.materials.insert_or_assign(name, material);
    }
    for (const auto& [name, texture] : car.looks.textures) {
        assets.textures.insert_or_assign(name, texture);
    }
}

}
