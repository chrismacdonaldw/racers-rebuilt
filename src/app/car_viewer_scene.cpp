#include "app/car_viewer_scene.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/racer/racer_lookup.hpp"
#include "app/track_model.hpp"
#include "assets/piece_catalog.hpp"
#include "core/log.hpp"
#include "core/quaternion.hpp"
#include "sim/car_build/built_car.hpp"
#include "sim/car_build/car_mesh.hpp"

namespace rr::app {
namespace {

constexpr sim::EntityId kCarId{1};
constexpr std::string_view kPieceFolder = "MENUDATA/PIECEDB";
constexpr std::string_view kCarModel = "CAR";
constexpr float kTurnRadiansPerTick = 0.01F;
constexpr Vector3 kUp{.y = 1.0F};
constexpr Transform kCameraTransform{.position = {.y = 5.0F, .z = 14.0F},
                                     .rotation = {.w = 0.9887711F, .x = -0.1494381F}};

float MiddleHeight(const assets::GeometryAsset& car) {
    const auto [low, high] = std::ranges::minmax(
        car.vertices, {}, [](const assets::GeometryVertex& vertex) { return vertex.position.z; });
    return (low.position.z + high.position.z) / 2.0F;
}

Result<assets::GeometryAsset> BuildCar(const assets::Vfs& vfs, std::string_view name) {
    const Result<assets::CarData> data =
        FindGameRacer(vfs, {.name = std::string(name)}).transform([](const settings::Racer& racer) {
            return racer.car;
        });
    const Result<assets::PieceCatalog> catalog = data.and_then([&](const assets::CarData& car) {
        return assets::PieceCatalog::Load(vfs, assets::PieceDetail::Menu)
            .transform([&](assets::PieceCatalog loaded) {
                loaded.ReadExtraModels(vfs, assets::PieceTypesOf(car));
                return loaded;
            });
    });
    Result<sim::CarMesh> mesh = catalog.and_then([&](const assets::PieceCatalog& pieces) {
        return sim::BuiltCar::Load(*data, pieces.library).and_then([&](const sim::BuiltCar& car) {
            return sim::BuildCarMesh(car, pieces);
        });
    });
    if (!mesh) {
        return std::unexpected(std::move(mesh.error()));
    }
    log::Info("{}: {} triangles, {} pieces", name, mesh->geometry.indices.size() / 3,
              mesh->drawnPieces);
    return std::move(mesh->geometry);
}

}

bool IsCarViewerScene(std::string_view scene) {
    return scene.starts_with(kCarViewerScenePrefix);
}

Result<std::unique_ptr<CarViewerScene>> CarViewerScene::Create(render::Renderer& renderer,
                                                               const assets::Vfs& vfs,
                                                               assets::TrackLoader& loader,
                                                               std::string_view scene) {
    Result<assets::GeometryAsset> car = BuildCar(vfs, scene.substr(kCarViewerScenePrefix.size()));
    const float middle = car && !car->vertices.empty() ? MiddleHeight(*car) : 0.0F;
    const std::array<std::string, 1> world{"LPIECEHI"};
    Result<assets::TrackAssets> loaded = car.and_then(
        [&](const assets::GeometryAsset&) { return loader.LoadWorlds(kPieceFolder, world); });
    Result<std::vector<render::MeshDraw>> draws = loaded.and_then([&](assets::TrackAssets& pieces) {
        pieces.models.insert_or_assign(
            std::string(kCarModel), std::make_shared<const assets::GeometryAsset>(std::move(*car)));
        return UploadModel(renderer, pieces, kCarModel);
    });
    if (!draws) {
        return std::unexpected(std::move(draws.error()));
    }
    return std::unique_ptr<CarViewerScene>(new CarViewerScene(renderer, std::move(*draws), middle));
}

void CarViewerScene::Tick(const sim::InputFrame& input) {
    const bool isSteering = input.players[0].IsHeld(sim::Action::SteerLeft) ||
                            input.players[0].IsHeld(sim::Action::SteerRight);
    turnRadians_ += kTurnRadiansPerTick * (isSteering ? 3.0F : 1.0F);
}

void CarViewerScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entities.assign(
        1, {.id = kCarId, .transform = {.rotation = FromAxisAngle(kUp, turnRadians_)}});
    out.camera = {.transform = {.position = {.x = kCameraTransform.position.x,
                                             .y = kCameraTransform.position.y + middleHeight_,
                                             .z = kCameraTransform.position.z},
                                .rotation = kCameraTransform.rotation}};
}

void CarViewerScene::Render(const sim::RenderSnapshot& interpolated) {
    if (!interpolated.entities.empty()) {
        for (render::MeshDraw& draw : draws_) {
            draw.transform = interpolated.entities.front().transform;
        }
    }
    const Result<void> drawn =
        renderer_->Draw({.camera = {.transform = interpolated.camera.transform}, .meshes = draws_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
    }
}

}
