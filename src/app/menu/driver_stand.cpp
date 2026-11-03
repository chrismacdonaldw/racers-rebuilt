#include "app/menu/driver_stand.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <utility>

#include "render/ui_layout.hpp"
#include "sim/animation/part_clock.hpp"

namespace rr::app {
namespace {

constexpr std::uint64_t kIdleSeed = 0xCB5EED;
constexpr std::string_view kModel = "EDITDRIVER";
constexpr std::size_t kHeadReaction = 0;
constexpr std::size_t kChestReaction = 1;
constexpr std::size_t kLegReaction = 2;
constexpr std::size_t kShakeReaction = 3;
constexpr render::Color kAmbient{.r = 128.0F / 255.0F, .g = 128.0F / 255.0F, .b = 128.0F / 255.0F};

}

Result<std::unique_ptr<DriverStand>> DriverStand::Load(render::Renderer& renderer,
                                                       const assets::Vfs& vfs,
                                                       assets::TrackLoader& loader,
                                                       const MenuFiles& files,
                                                       std::string_view screen) {
    Result<StandScene> scene = LoadStandScene(vfs, loader, files, screen, kBuilderStandSet);
    Result<TrackModel> model = scene.and_then(
        [&](const StandScene& loaded) { return UploadTrack(renderer, loaded.assets); });
    Result<StandFraming> framing = model.and_then([&](const TrackModel&) {
        return LoadStandFraming(renderer, vfs, files, screen, kBuilderStandSet);
    });
    if (!framing) {
        return Fail(framing.error().code,
                    std::format("{} driver stand: {}", screen, framing.error().message));
    }
    return std::unique_ptr<DriverStand>(new DriverStand(
        std::make_unique<StandScene>(std::move(*scene)), std::move(*model), *framing));
}

DriverStand::DriverStand(std::unique_ptr<StandScene> scene, TrackModel model,
                         const StandFraming& framing)
    : scene_(std::move(scene)),
      model_(std::move(model)),
      framing_(framing),
      entities_(scene_->sources),
      jointed_(model_.animatedModels, scene_->sources),
      random_(kIdleSeed) {
    light_.front() = MenuSceneLight();
}

Result<void> DriverStand::Show(KitMinifig minifig) {
    Result<std::size_t> skeleton = SkeletonOf(*minifig.skeleton);
    if (!skeleton) {
        return std::unexpected(std::move(skeleton.error()));
    }
    sim::TrackEntity& driver = scene_->sources.entities.at(scene_->driver);
    driver.model = std::string(kModel);
    driver.skeleton = *skeleton;
    driver.scale = minifig.scale;
    model_.animatedModels.insert_or_assign(std::string(kModel), std::move(minifig.model));
    jointed_ = TrackEntityDraws(model_.animatedModels, scene_->sources);
    if (!isShown_) {
        isShown_ = true;
        PlayNext();
    }
    return {};
}

void DriverStand::React(DriverSlot slot) {
    switch (slot) {
        case DriverSlot::Hat:
        case DriverSlot::Face:
            pending_ = scene_->reactions.at(kHeadReaction);
            break;
        case DriverSlot::Torso:
            pending_ = scene_->reactions.at(kChestReaction);
            break;
        case DriverSlot::Legs:
            pending_ = scene_->reactions.at(random_.Below(2) != 0 ? kShakeReaction : kLegReaction);
            break;
    }
}

void DriverStand::Leave() {
    isLeaving_ = true;
    if (isShown_) {
        const auto count = static_cast<std::uint32_t>(scene_->cheers.size());
        entities_.Play(scene_->driver, scene_->cheers.at(random_.Below(count)), false);
    }
}

bool DriverStand::IsLeaving() const {
    return isLeaving_ && isShown_ && !entities_.IsHoldingLastFrame(scene_->driver);
}

void DriverStand::Tick(Milliseconds elapsed) {
    entities_.Tick(elapsed);
    if (isShown_ && !isLeaving_ && entities_.IsHoldingLastFrame(scene_->driver)) {
        PlayNext();
    }
}

render::RenderView DriverStand::Render(std::span<const sim::EntityClock> clocks) {
    jointed_.Update(clocks);
    draws_.assign(model_.draws.begin(), model_.draws.end());
    if (isShown_) {
        draws_.insert(draws_.end(), jointed_.Draws().begin(), jointed_.Draws().end());
    }
    return {.camera = scene_->camera,
            .lighting = {.lights = light_, .ambient = kAmbient},
            .meshes = draws_,
            .viewport = render::UiViewport(framing_.interior)};
}

Result<std::size_t> DriverStand::SkeletonOf(const assets::SkeletonAsset& skeleton) {
    const auto found = std::ranges::find(skeletons_, &skeleton, &AddedSkeleton::skeleton);
    if (found != skeletons_.end()) {
        return found->index;
    }
    const sim::TrackEntity& driver = scene_->sources.entities.at(scene_->driver);
    const assets::AnimationAsset& animations = scene_->sources.animations.at(driver.animation);
    return sim::CheckPartTracks(animations, skeleton.nodes.size()).transform([&] {
        scene_->sources.skeletons.push_back(skeleton);
        const std::size_t index = scene_->sources.skeletons.size() - 1;
        skeletons_.push_back({.skeleton = &skeleton, .index = index});
        return index;
    });
}

void DriverStand::PlayNext() {
    const std::optional<std::size_t> current = entities_.CurrentPart(scene_->driver);
    const auto count = static_cast<std::uint32_t>(scene_->idles.size());
    std::optional<std::size_t> next = std::exchange(pending_, std::nullopt);
    while (!next || (next == current && count > 1)) {
        next = scene_->idles.at(random_.Below(count));
    }
    entities_.Play(scene_->driver, *next, false);
}

}
