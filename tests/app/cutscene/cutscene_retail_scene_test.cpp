#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/cutscene/cutscene_bindings.hpp"
#include "app/cutscene/cutscene_materials.hpp"
#include "app/cutscene/cutscene_player.hpp"
#include "app/frame_loop.hpp"
#include "assets/archive_survey.hpp"
#include "assets/loader_pool.hpp"
#include "assets/track_loader.hpp"

namespace rr::app {
namespace {

struct RetailCutscene {
    std::string_view folder;
    std::string_view file;
};

constexpr std::array kCutscenes{
    RetailCutscene{.folder = "LEGAL", .file = "SPLASH1"},
    RetailCutscene{.folder = "CIRCUIT1", .file = "CIR1-01"},
    RetailCutscene{.folder = "CIRCUIT2", .file = "TEST"},
    RetailCutscene{.folder = "CIRCUIT3", .file = "CIR3-01"},
    RetailCutscene{.folder = "CIRCUIT4", .file = "CIR4-01"},
    RetailCutscene{.folder = "CIRCUIT5", .file = "CIR5-01"},
    RetailCutscene{.folder = "CIRCUIT6", .file = "TEST"},
    RetailCutscene{.folder = "CIRCUIT7", .file = "FR"},
    RetailCutscene{.folder = "C_AWARD1", .file = "1STPLACE"},
    RetailCutscene{.folder = "C_AWARD2", .file = "TWO"},
    RetailCutscene{.folder = "C_AWARD3", .file = "3-01"},
    RetailCutscene{.folder = "C_AWARD4", .file = "LOSE"},
    RetailCutscene{.folder = "WINCAR", .file = "WINCAR"},
    RetailCutscene{.folder = "WINRRCAR", .file = "END"},
    RetailCutscene{.folder = "WINVVCAR", .file = "VVWIN"},
    RetailCutscene{.folder = "CREDITS", .file = "CREDITS"},
    RetailCutscene{.folder = "SINGRACE", .file = "PST"},
};

using MaterialMap = assets::NameMap<render::Material>;

bool IsFinite(const Vector3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

struct LoadedScene {
    std::vector<const assets::WorldAsset*> worlds;
    sim::TrackEntitySources sources;
    WorldAnimations animations;
    MaterialMap materials;
};

CutscenePlayer Start(const assets::CutsceneFrame& frame, LoadedScene& scene) {
    Result<CutsceneBindings> bindings = BindCutscene(frame, scene.worlds, {}, scene.sources);
    REQUIRE_MESSAGE(bindings.has_value(), bindings.error().message);
    Result<CutsceneMaterials> tracks =
        CutsceneMaterials::Create(frame, bindings->models, scene.animations, scene.materials);
    REQUIRE_MESSAGE(tracks.has_value(), tracks.error().message);
    Result<CutsceneSprites> sprites = CutsceneSprites::Create(bindings->models, scene.materials);
    REQUIRE_MESSAGE(sprites.has_value(), sprites.error().message);
    return {frame,
            std::move(*bindings),
            {.materials = std::move(*tracks), .sprites = std::move(*sprites)},
            scene.sources,
            {},
            false};
}

void PlayToEnd(const assets::CutsceneFrame& frame, LoadedScene& scene) {
    CAPTURE(frame.name);
    CutscenePlayer player = Start(frame, scene);
    const std::int64_t ticks =
        (std::int64_t{frame.length} * 1000 / frame.framesPerSecond / kSimTick.count()) + 2;
    for (std::int64_t tick = 0; tick < ticks && !player.IsComplete(); ++tick) {
        player.Advance(kSimTick);
        if (const std::optional<render::Camera> camera = player.ViewCamera(player.Clocks())) {
            REQUIRE(IsFinite(camera->transform.position));
        }
    }
    CHECK(player.IsComplete());
}

void CheckCutscene(const assets::Vfs& vfs, assets::TrackLoader& loader,
                   const RetailCutscene& cutscene) {
    CAPTURE(cutscene.file);
    const std::string folder = std::format("MENUDATA/{}", cutscene.folder);
    const Result<assets::CutsceneTimelineAsset> timeline =
        vfs.Read(std::format("{}/{}.CDB", folder, cutscene.file))
            .and_then([](const assets::FileData& data) {
                return assets::ParseCutsceneTimeline(data.Bytes());
            });
    REQUIRE(timeline.has_value());
    const Result<assets::TrackAssets> assets = loader.LoadWorlds(folder, timeline->worlds);
    REQUIRE_MESSAGE(assets.has_value(), assets.error().message);
    Result<sim::TrackEntitySources> sources = sim::LoadWorldEntities(vfs, folder, timeline->worlds);
    REQUIRE_MESSAGE(sources.has_value(), sources.error().message);
    LoadedScene scene{.sources = std::move(*sources)};
    for (const auto& world : assets->worlds) {
        scene.worlds.push_back(world.get());
    }
    Result<WorldAnimations> animations = LoadWorldAnimations(vfs, folder, scene.worlds);
    REQUIRE_MESSAGE(animations.has_value(), animations.error().message);
    scene.animations = std::move(*animations);
    for (const auto& entry : assets->materials) {
        scene.materials.emplace(entry.first, render::Material{});
    }
    for (const assets::CutsceneFrame& frame : timeline->frames) {
        PlayToEnd(frame, scene);
    }
}

TEST_CASE("every retail cutscene binds to its worlds and plays to its end") {
    const std::optional<assets::Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    assets::LoaderPool pool(2);
    assets::TrackLoader loader(*vfs, pool);
    for (const RetailCutscene& cutscene : kCutscenes) {
        CheckCutscene(*vfs, loader, cutscene);
    }
}

}
}
