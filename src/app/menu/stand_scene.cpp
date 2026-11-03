#include "app/menu/stand_scene.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <variant>

#include "app/menu/menu_widgets.hpp"
#include "app/track_space.hpp"
#include "assets/animation_database.hpp"
#include "assets/game_path.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kFolder = "MENUDATA/RS_SET";
constexpr std::string_view kShowcase = "showcase";
constexpr std::string_view kBlendedWorld = "BLENDED";
constexpr std::string_view kTurntable = "crsdow";
constexpr std::string_view kDriverEntity = "driver";
constexpr std::string_view kAnimations = "MENUDATA/RSANIM.ADB";
constexpr std::array<std::string_view, 7> kIdles{"breath1", "breath2", "swayf", "watch",
                                                 "hips",    "tapfoot", "hey!"};
constexpr std::array<std::string_view, 3> kCheers{"jump", "yes!", "spin"};
constexpr Vector3 kCarSpot{.x = -11.52F, .y = -6.767F, .z = 0.0F};
constexpr Vector3 kDriverSpot{.x = -0.938F, .y = -0.898F, .z = 1.487F};
constexpr Vector3 kDriverFacing{.x = 0.963631F, .y = -0.267238F, .z = 0.0F};
constexpr Vector3 kUp{.z = 1.0F};

Quaternion Facing(const Vector3& forward, const Vector3& up) {
    return sim::ToQuaternion(sim::OrthonormalBasis(forward, up));
}

Result<std::vector<std::string>> WorldsOf(const assets::MenuElement& showcase,
                                          std::string_view screen) {
    const auto* scene = std::get_if<assets::SceneElement>(&showcase.body);
    if (scene == nullptr) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{}.MIB {} isn't a scene", screen, kShowcase));
    }
    std::vector<std::string> worlds{assets::ToUpperAscii(scene->world)};
    if (scene->worldFlags && scene->worldFlags->at(1) != 0) {
        worlds.emplace_back(kBlendedWorld);
    }
    return worlds;
}

Result<render::Camera> StandCamera(const assets::TrackAssets& assets) {
    if (assets.worlds.empty() || assets.worlds.front()->cameras.empty()) {
        return Fail(ErrorCode::NotFound, std::format("{} has no camera", kFolder));
    }
    const assets::WorldCamera& camera = assets.worlds.front()->cameras.front();
    const Vector3 forward = camera.target ? *camera.target - camera.position : camera.forward;
    return render::Camera{.transform = ViewToEngine({.position = camera.position,
                                                     .rotation = Facing(forward, camera.up)}),
                          .verticalFovDegrees = camera.fovDegrees,
                          .nearPlane = camera.nearClip,
                          .farPlane = camera.farClip};
}

Result<std::vector<std::size_t>> PartsNamed(const assets::AnimationAsset& animation,
                                            std::span<const std::string_view> names) {
    std::vector<std::size_t> parts;
    parts.reserve(names.size());
    for (const std::string_view name : names) {
        const auto part = std::ranges::find(animation.parts, name, &assets::AnimationPart::name);
        if (part == animation.parts.end()) {
            return Fail(ErrorCode::NotFound, std::format("{} has no part {}", kAnimations, name));
        }
        parts.push_back(static_cast<std::size_t>(part - animation.parts.begin()));
    }
    return parts;
}

Result<std::size_t> FindEntity(const sim::TrackEntitySources& sources, std::string_view name) {
    const auto entity = std::ranges::find(sources.entities, name, &sim::TrackEntity::name);
    if (entity == sources.entities.end()) {
        return Fail(ErrorCode::NotFound, std::format("{} has no {}", kFolder, name));
    }
    return static_cast<std::size_t>(entity - sources.entities.begin());
}

Result<void> AddDriver(StandScene& scene, assets::AnimationAsset animations) {
    Result<void> parts =
        PartsNamed(animations, kIdles)
            .and_then([&](std::vector<std::size_t> idles) {
                scene.idles = std::move(idles);
                return PartsNamed(animations, kCheers);
            })
            .transform([&](std::vector<std::size_t> cheers) { scene.cheers = std::move(cheers); });
    if (!parts) {
        return parts;
    }
    sim::TrackEntitySources& sources = scene.sources;
    sources.animations.push_back(std::move(animations));
    sources.entities.push_back(
        {.name = std::string(kDriverEntity),
         .animation = sources.animations.size() - 1,
         .placement = {.position = kDriverSpot, .rotation = Facing(kDriverFacing, kUp)}});
    scene.driver = sources.entities.size() - 1;
    return {};
}

}

Result<const assets::MenuElement*> FindShowcase(const MenuFiles& files, std::string_view screen) {
    return FindMenuElement(files, screen, kShowcase);
}

Result<StandScene> LoadStandScene(const assets::Vfs& vfs, assets::TrackLoader& loader,
                                  const MenuFiles& files, std::string_view screen) {
    Result<std::vector<std::string>> worlds =
        FindShowcase(files, screen).and_then([&](const assets::MenuElement* showcase) {
            return WorldsOf(*showcase, screen);
        });
    Result<assets::TrackAssets> assets = worlds.and_then(
        [&](const std::vector<std::string>& names) { return loader.LoadWorlds(kFolder, names); });
    Result<render::Camera> camera = assets.and_then(StandCamera);
    Result<sim::TrackEntitySources> sources = camera.and_then(
        [&](const render::Camera&) { return sim::LoadWorldEntities(vfs, kFolder, *worlds); });
    Result<std::size_t> turntable = sources.and_then(
        [](const sim::TrackEntitySources& loaded) { return FindEntity(loaded, kTurntable); });
    Result<assets::AnimationAsset> animations =
        turntable.and_then([&](std::size_t) { return vfs.Read(kAnimations); })
            .and_then(
                [](const assets::FileData& data) { return assets::ParseAnimations(data.Bytes()); });
    if (!animations) {
        return std::unexpected(std::move(animations.error()));
    }
    StandScene scene{.assets = std::move(*assets),
                     .sources = std::move(*sources),
                     .camera = *camera,
                     .turntable = *turntable};
    return AddDriver(scene, std::move(*animations)).transform([&] { return std::move(scene); });
}

Transform StandCarPlacement(const sim::TrackEntitySources& sources, std::size_t turntable,
                            const sim::EntityClock& clock) {
    return {.position = kCarSpot,
            .rotation = sim::NodeWorld(sources, turntable, clock, 0).rotation};
}

}
