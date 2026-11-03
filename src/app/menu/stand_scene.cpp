#include "app/menu/stand_scene.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <utility>
#include <variant>

#include "app/menu/menu_page.hpp"
#include "app/menu/menu_widgets.hpp"
#include "app/track_space.hpp"
#include "assets/animation_database.hpp"
#include "assets/game_path.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kBlendedWorld = "BLENDED";
constexpr std::string_view kDriverEntity = "driver";
constexpr std::array<std::string_view, 7> kRacerIdles{"breath1", "breath2", "swayf", "watch",
                                                      "hips",    "tapfoot", "hey!"};
constexpr std::array<std::string_view, 3> kRacerCheers{"jump", "yes!", "spin"};
constexpr std::array<std::string_view, 8> kBuilderIdles{"breath1", "breath2", "sway",  "check",
                                                        "look",    "kick",    "watch", "hips"};
constexpr std::array<std::string_view, 2> kBuilderCheers{"jump", "kiss"};
constexpr std::array<std::string_view, 4> kBuilderReactions{"head1", "chest", "leg", "shaking"};
constexpr std::array<std::string_view, 1> kLicencePoses{"a0"};
constexpr float kLevel = 1.0F / 255.0F;
constexpr Vector3 kCarSpot{.x = -11.52F, .y = -6.767F, .z = 0.0F};
constexpr Vector3 kLightTravel{.x = -1.0F, .y = -1.0F, .z = -1.0F};

Quaternion Facing(const Vector3& forward, const Vector3& up) {
    return sim::ToQuaternion(sim::OrthonormalBasis(forward, up));
}

Result<std::vector<std::string>> WorldsOf(const assets::MenuElement& element,
                                          std::string_view screen) {
    const auto* scene = std::get_if<assets::SceneElement>(&element.body);
    if (scene == nullptr) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}.MIB {} isn't a scene", screen, element.name));
    }
    std::vector<std::string> worlds{assets::ToUpperAscii(scene->world)};
    if (scene->worldFlags && scene->worldFlags->at(1) != 0) {
        worlds.emplace_back(kBlendedWorld);
    }
    return worlds;
}

Result<render::Camera> StandCamera(const assets::TrackAssets& assets, std::string_view folder) {
    if (assets.worlds.empty() || assets.worlds.front()->cameras.empty()) {
        return Fail(ErrorCode::NotFound, std::format("{} has no camera", folder));
    }
    return MenuSceneCamera(assets.worlds.front()->cameras.front());
}

Result<std::vector<std::size_t>> PartsNamed(const assets::AnimationAsset& animation,
                                            std::span<const std::string_view> names,
                                            std::string_view file) {
    std::vector<std::size_t> parts;
    parts.reserve(names.size());
    for (const std::string_view name : names) {
        const auto part = std::ranges::find(animation.parts, name, &assets::AnimationPart::name);
        if (part == animation.parts.end()) {
            return Fail(ErrorCode::NotFound, std::format("{} has no part {}", file, name));
        }
        parts.push_back(static_cast<std::size_t>(part - animation.parts.begin()));
    }
    return parts;
}

Result<std::optional<std::size_t>> FindTurntable(const sim::TrackEntitySources& sources,
                                                 const StandSet& set) {
    if (set.turntable.empty()) {
        return std::nullopt;
    }
    const auto entity = std::ranges::find(sources.entities, set.turntable, &sim::TrackEntity::name);
    if (entity == sources.entities.end()) {
        return Fail(ErrorCode::NotFound, std::format("{} has no {}", set.folder, set.turntable));
    }
    return static_cast<std::size_t>(entity - sources.entities.begin());
}

Result<const assets::FrameElement*> LinkedFrame(const MenuFiles& files, std::string_view screen,
                                                const assets::MenuElement& element) {
    const auto* scene = std::get_if<assets::SceneElement>(&element.body);
    if (scene == nullptr) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}.MIB {} isn't a scene", screen, element.name));
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

Result<void> AddDriver(StandScene& scene, assets::AnimationAsset animations, const StandSet& set) {
    Result<void> parts = PartsNamed(animations, set.idles, set.animations)
                             .and_then([&](std::vector<std::size_t> idles) {
                                 scene.idles = std::move(idles);
                                 return PartsNamed(animations, set.cheers, set.animations);
                             })
                             .and_then([&](std::vector<std::size_t> cheers) {
                                 scene.cheers = std::move(cheers);
                                 return PartsNamed(animations, set.reactions, set.animations);
                             })
                             .transform([&](std::vector<std::size_t> reactions) {
                                 scene.reactions = std::move(reactions);
                             });
    if (!parts) {
        return parts;
    }
    sim::TrackEntitySources& sources = scene.sources;
    sources.animations.push_back(std::move(animations));
    sources.entities.push_back({.name = std::string(kDriverEntity),
                                .animation = sources.animations.size() - 1,
                                .placement = {.position = set.driverSpot,
                                              .rotation = Facing(set.driverFacing, set.driverUp)}});
    scene.driver = sources.entities.size() - 1;
    return {};
}

}

render::DirectionalLight MenuSceneLight() {
    return {.direction = ToEngine(Normalized(kLightTravel, kLightTravel))};
}

render::Camera MenuSceneCamera(const assets::WorldCamera& camera) {
    const Vector3 forward = camera.target ? *camera.target - camera.position : camera.forward;
    return {.transform =
                ViewToEngine({.position = camera.position, .rotation = Facing(forward, camera.up)}),
            .verticalFovDegrees = camera.fovDegrees,
            .nearPlane = camera.nearClip,
            .farPlane = camera.farClip};
}

const StandSet kRacerStandSet{.folder = "MENUDATA/RS_SET",
                              .element = "showcase",
                              .animations = "MENUDATA/RSANIM.ADB",
                              .idles = kRacerIdles,
                              .cheers = kRacerCheers,
                              .turntable = "crsdow",
                              .driverSpot = {.x = -0.938F, .y = -0.898F, .z = 1.487F},
                              .driverFacing = {.x = 0.963631F, .y = -0.267238F, .z = 0.0F},
                              .ambient = 120.0F * kLevel};

const StandSet kBuilderStandSet{.folder = "MENUDATA/CB_SET",
                                .element = "platform",
                                .animations = "MENUDATA/CBANIM.ADB",
                                .idles = kBuilderIdles,
                                .cheers = kBuilderCheers,
                                .reactions = kBuilderReactions,
                                .driverSpot = {.x = -5.359F, .y = -3.15F, .z = 0.026F},
                                .driverFacing = {.x = 1.0F},
                                .ambient = 128.0F * kLevel};

const StandSet kLicenceStandSet{.folder = "MENUDATA",
                                .element = "platform",
                                .animations = "MENUDATA/CMAMAN.ADB",
                                .idles = kLicencePoses,
                                .driverSpot = {.x = -5.359F, .y = -3.15F, .z = 0.0F},
                                .driverFacing = {.x = 1.0F},
                                .driverUp = {.y = -0.0349F, .z = 0.99939F},
                                .ambient = 120.0F * kLevel,
                                .light = 180.0F * kLevel};

Result<const assets::MenuElement*> FindStandElement(const MenuFiles& files, std::string_view screen,
                                                    const StandSet& set) {
    return FindMenuElement(files, screen, set.element);
}

Result<StandFraming> LoadStandFraming(render::Renderer& renderer, const assets::Vfs& vfs,
                                      const MenuFiles& files, std::string_view screen,
                                      const StandSet& set) {
    MenuImageCache images(renderer, vfs, files);
    const Result<const assets::MenuElement*> element = FindStandElement(files, screen, set);
    return element
        .and_then(
            [&](const assets::MenuElement* found) { return LinkedFrame(files, screen, *found); })
        .and_then([&](const assets::FrameElement* frame) { return LoadMenuFrame(images, *frame); })
        .transform([&](const MenuFrameLook& look) {
            const assets::MenuElement& found = **element;
            const render::Rect frame = SpanOf(OriginOf(files.layout, found), found.widget.rect);
            return StandFraming{
                .look = look, .frame = frame, .interior = FrameInterior(look, frame)};
        });
}

Result<StandScene> LoadStandScene(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                  const MenuFiles& files, std::string_view screen,
                                  const StandSet& set) {
    Result<std::vector<std::string>> worlds =
        FindStandElement(files, screen, set).and_then([&](const assets::MenuElement* element) {
            return WorldsOf(*element, screen);
        });
    Result<assets::TrackAssets> assets =
        worlds.and_then([&](const std::vector<std::string>& names) {
            return loader.LoadWorlds(set.folder, names);
        });
    Result<render::Camera> camera = assets.and_then(
        [&](const assets::TrackAssets& loaded) { return StandCamera(loaded, set.folder); });
    Result<sim::TrackEntitySources> sources = camera.and_then(
        [&](const render::Camera&) { return sim::LoadWorldEntities(vfs, set.folder, *worlds); });
    Result<std::optional<std::size_t>> turntable = sources.and_then(
        [&](const sim::TrackEntitySources& loaded) { return FindTurntable(loaded, set); });
    Result<assets::AnimationAsset> animations =
        turntable
            .and_then([&](const std::optional<std::size_t>&) { return vfs.Read(set.animations); })
            .and_then(
                [](const assets::FileData& data) { return assets::ParseAnimations(data.Bytes()); });
    if (!animations) {
        return std::unexpected(std::move(animations.error()));
    }
    StandScene scene{.assets = std::move(*assets),
                     .sources = std::move(*sources),
                     .camera = *camera,
                     .turntable = *turntable};
    return AddDriver(scene, std::move(*animations), set).transform([&] {
        return std::move(scene);
    });
}

Transform StandCarPlacement(const sim::TrackEntitySources& sources, std::size_t turntable,
                            const sim::EntityClock& clock) {
    return {.position = kCarSpot,
            .rotation = sim::NodeWorld(sources, turntable, clock, 0).rotation};
}

}
