#include "app/menu/menu_champion.hpp"

#include <algorithm>
#include <format>
#include <utility>
#include <variant>

#include "app/circuit_awards.hpp"
#include "app/cutscene/award_driver.hpp"
#include "app/menu/menu_backdrop.hpp"
#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/menu/stand_racer.hpp"
#include "app/menu/stand_scene.hpp"
#include "assets/animation_database.hpp"
#include "assets/game_path.hpp"
#include "assets/loader_pool.hpp"
#include "assets/racer_catalog.hpp"
#include "assets/world_database.hpp"
#include "core/log.hpp"
#include "render/ui_layout.hpp"
#include "settings/progress.hpp"
#include "sim/animation/part_clock.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kScreen = "MAINMENU";
constexpr std::string_view kPlatform = "platform";
constexpr std::string_view kAnimations = "MENUDATA/LEGOMAN.ADB";
constexpr std::string_view kModel = "CHAMPION";
constexpr std::size_t kLoaderWorkers = 2;
constexpr std::size_t kChampion = 0;
constexpr std::size_t kEnterPart = 0;
constexpr std::size_t kIdlePart = 1;
constexpr std::size_t kTapPart = 2;
constexpr std::size_t kParts = 3;
constexpr float kTapFrame = 208.0F;
constexpr Milliseconds kTapInterval{30000};
constexpr Vector3 kSpot{.x = 18.181229F, .y = -10.622759F, .z = 0.025708F};
constexpr Vector3 kFacing{.x = 0.97237F, .y = -0.233445F, .z = 0.0F};
constexpr Vector3 kUp{.z = 1.0F};
constexpr std::array<std::string_view, 7> kChampions{"CR", "KK", "BB", "JT", "BVB", "GM", "RR"};

Result<const assets::SceneElement*> PlatformScene(const MenuFiles& files) {
    return FindMenuElement(files, kScreen, kPlatform)
        .and_then([](const assets::MenuElement* element) -> Result<const assets::SceneElement*> {
            const auto* scene = std::get_if<assets::SceneElement>(&element->body);
            if (scene == nullptr) {
                return Fail(ErrorCode::InvalidData,
                            std::format("{}.MIB {} isn't a scene", kScreen, kPlatform));
            }
            return scene;
        });
}

Result<render::Camera> WorldCamera(const assets::Vfs& vfs, const assets::SceneElement& scene) {
    const std::string path = std::format("MENUDATA/{}.WDB", assets::ToUpperAscii(scene.world));
    return vfs.Read(path)
        .and_then([](const assets::FileData& data) { return assets::ParseWorld(data.Bytes()); })
        .and_then([&](const assets::WorldAsset& world) -> Result<render::Camera> {
            if (world.cameras.empty()) {
                return Fail(ErrorCode::NotFound, std::format("{} has no camera", path));
            }
            return MenuSceneCamera(world.cameras.front());
        });
}

Result<assets::AnimationAsset> LoadAnimations(const assets::Vfs& vfs, std::size_t nodeCount) {
    return vfs.Read(kAnimations)
        .and_then(
            [](const assets::FileData& data) { return assets::ParseAnimations(data.Bytes()); })
        .and_then([&](assets::AnimationAsset animations) -> Result<assets::AnimationAsset> {
            if (animations.parts.size() < kParts) {
                return Fail(ErrorCode::InvalidData,
                            std::format("{} has {} parts, not {}", kAnimations,
                                        animations.parts.size(), kParts));
            }
            return sim::CheckPartTracks(animations, nodeCount).transform([&] {
                return std::move(animations);
            });
        });
}

Result<StandRacer> BuildChampion(render::Renderer& renderer, const assets::Vfs& vfs,
                                 std::string_view champion) {
    assets::LoaderPool pool(kLoaderWorkers);
    assets::TrackLoader loader(vfs, pool);
    return assets::RacerCatalog::Load(vfs)
        .and_then([&](const assets::RacerCatalog& catalog) { return catalog.Find(champion); })
        .and_then([](const assets::RacerFiles& racer) { return DriverLookOf(racer.driver); })
        .and_then([&](const assets::DriverLook& look) {
            return BuildStandDriver(renderer, vfs, loader, look);
        });
}

}

std::string MenuChampionCode(const assets::RaceCatalog& races,
                             const std::set<std::string>& openCircuits) {
    std::size_t last = 0;
    for (const std::size_t circuit : races.Story()) {
        if (circuit != 0 && !openCircuits.contains(races.Circuits()[circuit].name)) {
            break;
        }
        last = circuit;
    }
    if (last < races.Circuits().size() && !races.Circuits()[last].boss.empty()) {
        return races.Circuits()[last].boss;
    }
    return std::string(last < kChampions.size() ? kChampions[last] : kChampions.front());
}

std::string SavedMenuChampion(const assets::Vfs& vfs, const std::filesystem::path& progressFile) {
    const Result<settings::Progress> progress = settings::LoadProgress(progressFile);
    const Result<assets::RaceCatalog> races = progress.and_then(
        [&](const settings::Progress&) { return assets::RaceCatalog::Load(vfs); });
    if (!races) {
        log::Warn("{}, the main menu shows the first champion", races.error().message);
        return std::string(kChampions.front());
    }
    return MenuChampionCode(*races, OpenCircuits(*races, progress->unlockedCircuits));
}

Result<std::unique_ptr<MenuChampion>> MenuChampion::Load(render::Renderer& renderer,
                                                         const assets::Vfs& vfs,
                                                         const MenuFiles& files,
                                                         std::string_view champion) {
    Result<const assets::SceneElement*> scene = PlatformScene(files);
    Result<render::Camera> camera = scene.and_then(
        [&](const assets::SceneElement* element) { return WorldCamera(vfs, *element); });
    Result<StandRacer> built = camera.and_then(
        [&](const render::Camera&) { return BuildChampion(renderer, vfs, champion); });
    Result<assets::AnimationAsset> animations = built.and_then(
        [&](const StandRacer& stand) { return LoadAnimations(vfs, stand.skeleton.nodes.size()); });
    if (!animations) {
        return Fail(animations.error().code,
                    std::format("main menu {}: {}", champion, animations.error().message));
    }
    const assets::MenuElement& platform = **FindMenuElement(files, kScreen, kPlatform);
    Parts parts{.camera = *camera,
                .bounds = SpanOf(OriginOf(files.layout, platform), platform.widget.rect)};
    parts.sources.skeletons.push_back(std::move(built->skeleton));
    parts.sources.animations.push_back(std::move(*animations));
    parts.sources.entities.push_back(
        {.name = std::string(kModel),
         .model = std::string(kModel),
         .placement = {.position = kSpot,
                       .rotation = sim::ToQuaternion(sim::OrthonormalBasis(kFacing, kUp))},
         .scale = built->scale});
    parts.models.insert_or_assign(std::string(kModel), std::move(built->driver));
    return std::unique_ptr<MenuChampion>(new MenuChampion(std::move(parts)));
}

MenuChampion::MenuChampion(Parts parts)
    : sources_(std::move(parts.sources)),
      models_(std::move(parts.models)),
      camera_(parts.camera),
      bounds_(parts.bounds),
      entities_(sources_),
      jointed_(models_, sources_),
      untilTap_(kTapInterval) {
    entities_.Play(kChampion, kEnterPart, false);
    light_.front() = MenuSceneLight();
}

bool MenuChampion::Tick(Milliseconds elapsed) {
    entities_.Tick(elapsed);
    bool taps = false;
    if (entities_.CurrentPart(kChampion) == kTapPart) {
        const bool isPastTap = entities_.Frame(kChampion) >= kTapFrame;
        taps = isPastTap && !hasTapped_;
        hasTapped_ = isPastTap;
    }
    untilTap_ -= std::min(elapsed, untilTap_);
    if (untilTap_ == Milliseconds{0}) {
        entities_.Play(kChampion, kTapPart, false);
        untilTap_ = kTapInterval;
    } else if (entities_.IsHoldingLastFrame(kChampion)) {
        entities_.Play(kChampion, kIdlePart, true);
    }
    return taps;
}

render::RenderView MenuChampion::Render(std::span<const sim::EntityClock> clocks,
                                        std::span<const MenuPicture> pictures) {
    jointed_.Update(clocks);
    draws_.assign(jointed_.Draws().begin(), jointed_.Draws().end());
    quads_.clear();
    AppendBackdropQuads(pictures, bounds_, camera_, quads_);
    return {.camera = camera_,
            .lighting = {.lights = light_, .ambient = kMenuSceneAmbient},
            .meshes = draws_,
            .quads = quads_,
            .viewport = render::UiViewport(bounds_)};
}

}
