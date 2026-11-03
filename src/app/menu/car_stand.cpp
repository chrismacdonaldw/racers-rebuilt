#include "app/menu/car_stand.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <utility>

#include "app/cutscene/award_car.hpp"
#include "app/menu/stand_racer.hpp"
#include "app/racer/racer_models.hpp"
#include "app/track_space.hpp"
#include "core/log.hpp"
#include "render/ui_layout.hpp"
#include "sim/animation/part_clock.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kLoaderWorkers = 2;
constexpr std::string_view kFolder = "MENUDATA/GARAGE";
constexpr std::string_view kElement = "garage";
constexpr std::string_view kDriverModel = "SEATED";
constexpr float kLevel = 120.0F / 255.0F;
constexpr render::Color kGrey{.r = kLevel, .g = kLevel, .b = kLevel};

}

Result<std::unique_ptr<CarStand>> CarStand::Load(render::Renderer& renderer, const assets::Vfs& vfs,
                                                 const MenuFiles& files, std::string_view screen,
                                                 const settings::Racer& racer) {
    auto pool = std::make_unique<assets::LoaderPool>(kLoaderWorkers);
    auto loader = std::make_unique<assets::TrackLoader>(vfs, *pool);
    Loading loading{.pool = std::move(pool), .loader = std::move(loader)};
    const Result<std::string> animations =
        LoadDriverParts(vfs).and_then([&](const assets::DriverPartCatalog& parts) {
            return MenuMinifigAnimations(parts, racer.look);
        });
    const StandSet set{.folder = kFolder,
                       .element = kElement,
                       .animations = animations ? std::string_view(*animations) : "",
                       .driverSpot = {},
                       .driverFacing = {.x = 1.0F}};
    Result<StandScene> scene = animations.and_then([&](const std::string&) {
        return LoadStandScene(vfs, *loading.loader, files, screen, set);
    });
    Result<TrackModel> model = scene.and_then(
        [&](const StandScene& loaded) { return UploadTrack(renderer, loaded.assets); });
    Result<StandFraming> framing = model.and_then(
        [&](const TrackModel&) { return LoadStandFraming(renderer, vfs, files, screen, set); });
    if (!framing) {
        return Fail(framing.error().code,
                    std::format("{} car stand: {}", screen, framing.error().message));
    }
    auto stand = std::unique_ptr<CarStand>(
        new CarStand(renderer, vfs, std::move(loading),
                     std::make_unique<StandScene>(std::move(*scene)), std::move(*model), *framing));
    if (Result<void> driver = stand->ShowDriver(racer); !driver) {
        log::Warn("{}, the car stands empty", driver.error().message);
    }
    stand->ShowCar(racer);
    return stand;
}

CarStand::CarStand(render::Renderer& renderer, const assets::Vfs& vfs, Loading loading,
                   std::unique_ptr<StandScene> scene, TrackModel model, const StandFraming& framing)
    : renderer_(&renderer),
      vfs_(&vfs),
      loading_(std::move(loading)),
      scene_(std::move(scene)),
      model_(std::move(model)),
      framing_(framing),
      entities_(scene_->sources),
      jointed_(model_.animatedModels, scene_->sources) {
    light_.front() = MenuSceneLight();
    light_.front().color = kGrey;
}

void CarStand::ShowCar(const settings::Racer& racer) {
    const auto cached = std::ranges::find_if(cars_, [&](const CarModel& built) {
        return built.chassis == racer.chassis && built.car == racer.car;
    });
    if (cached != cars_.end()) {
        shown_ = static_cast<std::size_t>(cached - cars_.begin());
        return;
    }
    Result<assets::ChassisEntry> chassis = FindMenuChassis(*vfs_, racer.chassis);
    Result<std::optional<JointedModel>> model = chassis.and_then([&](const assets::ChassisEntry&) {
        return BuildStandCar(*renderer_, *vfs_, *loading_.loader, racer, CarBody::Bricks);
    });
    shown_.reset();
    if (!model || !*model) {
        log::Warn("{}, the garage shows no car",
                  model ? std::format("{} has no pieces", racer.name) : model.error().message);
        return;
    }
    cars_.push_back({.chassis = racer.chassis,
                     .car = racer.car,
                     .model = std::move(**model),
                     .seat = chassis->seat});
    shown_ = cars_.size() - 1;
}

void CarStand::Tick(Milliseconds elapsed) {
    entities_.Tick(elapsed);
}

render::RenderView CarStand::Render(std::span<const sim::EntityClock> clocks,
                                    const Transform& car) {
    DrawCar(car);
    if (shown_) {
        const CarModel& built = cars_.at(*shown_);
        scene_->sources.entities.at(scene_->driver).placement = {.position = Apply(car, built.seat),
                                                                 .rotation = car.rotation};
        jointed_.Update(clocks);
        if (isDriverShown_) {
            draws_.insert(draws_.end(), jointed_.Draws().begin(), jointed_.Draws().end());
        }
    }
    light_.front().color = kGrey;
    return {.camera = scene_->camera,
            .lighting = {.lights = light_, .ambient = kGrey},
            .meshes = draws_,
            .viewport = render::UiViewport(framing_.interior)};
}

render::RenderView CarStand::Render(const Transform& car, const StandShot& shot) {
    DrawCar(car, shot.carAmbient);
    for (render::MeshDraw draw : shot.extras) {
        draw.transform = TransformToEngine(Compose(car, draw.transform));
        draws_.push_back(draw);
    }
    light_.front().color = shot.light;
    return {.camera = shot.camera,
            .lighting = {.lights = light_, .ambient = kGrey},
            .meshes = draws_,
            .viewport = render::UiViewport(shot.interior)};
}

void CarStand::DrawCar(const Transform& car, const render::Color& ambient) {
    draws_.assign(model_.draws.begin(), model_.draws.end());
    if (!shown_) {
        return;
    }
    const Transform placed = TransformToEngine(car);
    for (render::MeshDraw draw : cars_.at(*shown_).model.draws) {
        draw.transform = placed;
        draw.material.ambient = draw.material.ambient * ambient;
        draws_.push_back(draw);
    }
}

Result<void> CarStand::ShowDriver(const settings::Racer& racer) {
    return BuildStandDriver(*renderer_, *vfs_, *loading_.loader, racer.look)
        .and_then([&](StandRacer driver) -> Result<void> {
            sim::TrackEntity& seated = scene_->sources.entities.at(scene_->driver);
            const assets::AnimationAsset& animations =
                scene_->sources.animations.at(seated.animation);
            return sim::CheckPartTracks(animations, driver.skeleton.nodes.size()).transform([&] {
                scene_->sources.skeletons.push_back(std::move(driver.skeleton));
                seated.model = std::string(kDriverModel);
                seated.skeleton = scene_->sources.skeletons.size() - 1;
                seated.scale = driver.scale;
                model_.animatedModels.insert_or_assign(std::string(kDriverModel),
                                                       std::move(driver.driver));
                jointed_ = TrackEntityDraws(model_.animatedModels, scene_->sources);
                entities_.Play(scene_->driver, 0, true);
                isDriverShown_ = true;
            });
        });
}

}
