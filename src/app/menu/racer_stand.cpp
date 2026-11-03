#include "app/menu/racer_stand.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <utility>
#include <variant>

#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/track_space.hpp"
#include "core/log.hpp"
#include "render/ui_layout.hpp"
#include "sim/animation/part_clock.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kLoaderWorkers = 2;
constexpr std::uint64_t kIdleSeed = 0x5EED;

Result<const assets::FrameElement*> LinkedFrame(const MenuFiles& files, std::string_view screen,
                                                const assets::MenuElement& showcase) {
    const auto* scene = std::get_if<assets::SceneElement>(&showcase.body);
    if (scene == nullptr) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}.MIB {} isn't a scene", screen, showcase.name));
    }
    return FindMenuElement(files, screen, scene->linkedElement)
        .and_then([&](const assets::MenuElement* linked) -> Result<const assets::FrameElement*> {
            const auto* frame = std::get_if<assets::FrameElement>(&linked->body);
            if (frame == nullptr) {
                return Fail(ErrorCode::InvalidData,
                            std::format("{}.MIB {} isn't a frame", screen, linked->name));
            }
            return frame;
        });
}

void AppendSpan(const TrackEntityDraws& jointed, DrawSpan span,
                std::vector<render::MeshDraw>& out) {
    const std::span<const render::MeshDraw> draws = jointed.Draws().subspan(span.first, span.count);
    out.insert(out.end(), draws.begin(), draws.end());
}

}

Result<std::unique_ptr<RacerStand>> RacerStand::Load(render::Renderer& renderer,
                                                     const assets::Vfs& vfs, const MenuFiles& files,
                                                     std::string_view screen) {
    auto pool = std::make_unique<assets::LoaderPool>(kLoaderWorkers);
    auto loader = std::make_unique<assets::TrackLoader>(vfs, *pool);
    Loading loading{.pool = std::move(pool), .loader = std::move(loader)};
    Result<StandScene> scene = LoadStandScene(vfs, *loading.loader, files, screen);
    Result<TrackModel> model = scene.and_then(
        [&](const StandScene& loaded) { return UploadTrack(renderer, loaded.assets); });
    Result<Framing> framing = model.and_then(
        [&](const TrackModel&) { return LoadFraming(renderer, vfs, files, screen); });
    if (!framing) {
        return Fail(framing.error().code,
                    std::format("{} racer stand: {}", screen, framing.error().message));
    }
    return std::unique_ptr<RacerStand>(new RacerStand(
        vfs, renderer, std::move(loading), std::make_unique<StandScene>(std::move(*scene)),
        std::move(*model), *framing));
}

Result<RacerStand::Framing> RacerStand::LoadFraming(render::Renderer& renderer,
                                                    const assets::Vfs& vfs, const MenuFiles& files,
                                                    std::string_view screen) {
    MenuImageCache images(renderer, vfs, files);
    const Result<const assets::MenuElement*> showcase = FindShowcase(files, screen);
    return showcase
        .and_then([&](const assets::MenuElement* element) {
            return LinkedFrame(files, screen, *element);
        })
        .and_then([&](const assets::FrameElement* frame) { return LoadMenuFrame(images, *frame); })
        .transform([&](const MenuFrameLook& look) {
            const assets::MenuElement& element = **showcase;
            const render::Rect frame = SpanOf(OriginOf(files.layout, element), element.widget.rect);
            const render::Rect interior = FrameInterior(look, frame);
            return Framing{.look = look, .frame = frame, .interior = interior};
        });
}

RacerStand::RacerStand(const assets::Vfs& vfs, render::Renderer& renderer, Loading loading,
                       std::unique_ptr<StandScene> scene, TrackModel model, const Framing& framing)
    : vfs_(&vfs),
      renderer_(&renderer),
      loading_(std::move(loading)),
      scene_(std::move(scene)),
      model_(std::move(model)),
      framing_(framing),
      entities_(scene_->sources),
      jointed_(model_.animatedModels, scene_->sources),
      random_(kIdleSeed) {
    entities_.Play(scene_->turntable, 0, true);
    light_.front() = MenuSceneLight();
}

void RacerStand::Show(const settings::Racer& racer) {
    const Built& built = Build(racer);
    if (!built.parts) {
        Clear();
        return;
    }
    sim::TrackEntity& driver = scene_->sources.entities.at(scene_->driver);
    driver.model = built.model;
    driver.skeleton = built.skeleton;
    driver.scale = built.parts->scale;
    jointed_ = TrackEntityDraws(model_.animatedModels, scene_->sources);
    shown_ = static_cast<std::size_t>(&built - built_.data());
    isCheering_ = false;
    PlayIdle();
}

void RacerStand::Clear() {
    shown_.reset();
    isCheering_ = false;
    scene_->sources.entities.at(scene_->driver).model.clear();
    jointed_ = TrackEntityDraws(model_.animatedModels, scene_->sources);
}

void RacerStand::Cheer() {
    if (!shown_) {
        return;
    }
    const auto count = static_cast<std::uint32_t>(scene_->cheers.size());
    entities_.Play(scene_->driver, scene_->cheers.at(random_.Below(count)), false);
    isCheering_ = true;
}

bool RacerStand::IsCheering() const {
    return isCheering_ && !entities_.IsHoldingLastFrame(scene_->driver);
}

void RacerStand::Tick(Milliseconds elapsed) {
    entities_.Tick(elapsed);
    if (shown_ && !isCheering_ && entities_.IsHoldingLastFrame(scene_->driver)) {
        PlayIdle();
    }
}

render::RenderView RacerStand::Render(std::span<const sim::EntityClock> clocks,
                                      std::vector<render::SpriteDraw>& sprites) {
    const render::Viewport viewport = render::UiViewport(framing_.interior);
    if (!shown_) {
        DrawMenuFrame(framing_.look, framing_.frame, sprites);
        return {.viewport = viewport};
    }
    jointed_.Update(clocks);
    draws_.assign(model_.draws.begin(), model_.draws.end());
    if (const std::optional<StandRacer>& parts = built_.at(*shown_).parts; parts && parts->car) {
        AppendCar(clocks, *parts->car);
    }
    AppendSpan(jointed_, jointed_.SpanOf(scene_->driver), draws_);
    for (std::size_t entity = 0; entity < scene_->driver; ++entity) {
        AppendSpan(jointed_, jointed_.SpanOf(entity), draws_);
    }
    DrawMenuFrameBorder(framing_.look, framing_.frame, sprites);
    return {.camera = scene_->camera,
            .lighting = {.lights = light_, .ambient = kMenuSceneAmbient},
            .meshes = draws_,
            .viewport = viewport};
}

const RacerStand::Built& RacerStand::Build(const settings::Racer& racer) {
    const auto found = std::ranges::find_if(built_, [&](const Built& built) {
        return built.look == racer.look && built.chassis == racer.chassis && built.car == racer.car;
    });
    if (found != built_.end()) {
        return *found;
    }
    Built built{.look = racer.look, .chassis = racer.chassis, .car = racer.car};
    Result<StandRacer> parts = BuildParts(racer);
    if (!parts) {
        log::Warn("{}, the racer stand leaves {} out", parts.error().message, racer.name);
        return built_.emplace_back(std::move(built));
    }
    built.model = std::format("STAND{}", built_.size());
    built.skeleton = scene_->sources.skeletons.size();
    scene_->sources.skeletons.push_back(parts->skeleton);
    model_.animatedModels.insert_or_assign(built.model, parts->driver);
    built.parts = std::move(*parts);
    return built_.emplace_back(std::move(built));
}

Result<StandRacer> RacerStand::BuildParts(const settings::Racer& racer) const {
    const assets::AnimationAsset& animations =
        scene_->sources.animations.at(scene_->sources.entities.at(scene_->driver).animation);
    return BuildStandRacer(*renderer_, *vfs_, *loading_.loader, racer)
        .and_then([&](StandRacer parts) -> Result<StandRacer> {
            return sim::CheckPartTracks(animations, parts.skeleton.nodes.size()).transform([&] {
                return std::move(parts);
            });
        });
}

void RacerStand::PlayIdle() {
    const std::optional<std::size_t> current = entities_.CurrentPart(scene_->driver);
    const auto count = static_cast<std::uint32_t>(scene_->idles.size());
    std::size_t idle = scene_->idles.at(random_.Below(count));
    while (idle == current && count > 1) {
        idle = scene_->idles.at(random_.Below(count));
    }
    entities_.Play(scene_->driver, idle, false);
}

void RacerStand::AppendCar(std::span<const sim::EntityClock> clocks, const JointedModel& car) {
    const sim::EntityClock clock =
        scene_->turntable < clocks.size() ? clocks[scene_->turntable] : sim::EntityClock{};
    const Transform placed =
        TransformToEngine(StandCarPlacement(scene_->sources, scene_->turntable, clock));
    for (render::MeshDraw draw : car.draws) {
        draw.transform = placed;
        draws_.push_back(draw);
    }
}

}
