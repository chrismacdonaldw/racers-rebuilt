#include "app/cutscene/cutscene_scene.hpp"

#include <format>
#include <optional>
#include <utility>

#include "app/cutscene/cutscene_bindings.hpp"
#include "app/cutscene/cutscene_materials.hpp"
#include "app/frame_loop.hpp"
#include "app/track_model.hpp"
#include "assets/game_path.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

struct CutscenePath {
    std::string folder;
    std::string file;
};

Result<CutscenePath> ParsePath(std::string_view scene) {
    const std::string_view path = scene.substr(kCutsceneScenePrefix.size());
    const std::size_t slash = path.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 == path.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} isn't {}<folder>/<file>", scene, kCutsceneScenePrefix));
    }
    return CutscenePath{
        .folder = std::format("MENUDATA/{}", assets::ToUpperAscii(path.substr(0, slash))),
        .file = assets::ToUpperAscii(path.substr(slash + 1))};
}

Result<std::unique_ptr<assets::CutsceneTimelineAsset>> ReadTimeline(const assets::Vfs& vfs,
                                                                    const CutscenePath& path) {
    const std::string file = std::format("{}/{}.CDB", path.folder, path.file);
    Result<assets::CutsceneTimelineAsset> timeline = vfs.Read(file).and_then(
        [](const assets::FileData& data) { return assets::ParseCutsceneTimeline(data.Bytes()); });
    if (!timeline) {
        return Fail(timeline.error().code, std::format("{}: {}", file, timeline.error().message));
    }
    if (timeline->frames.empty()) {
        return Fail(ErrorCode::InvalidData, std::format("{} has no frames", file));
    }
    return std::make_unique<assets::CutsceneTimelineAsset>(std::move(*timeline));
}

std::vector<const assets::WorldAsset*> WorldsOf(const assets::TrackAssets& assets) {
    std::vector<const assets::WorldAsset*> worlds;
    worlds.reserve(assets.worlds.size());
    for (const assets::AssetCache<assets::WorldAsset>::Handle& world : assets.worlds) {
        worlds.push_back(world.get());
    }
    return worlds;
}

Result<CutsceneLooks> LoadLooks(const assets::Vfs& vfs, std::string_view folder,
                                std::span<const assets::WorldAsset* const> worlds,
                                const assets::CutsceneFrame& frame,
                                std::span<const ModelBinding> models,
                                const assets::NameMap<render::Material>& materials) {
    Result<CutsceneSprites> sprites = CutsceneSprites::Create(models, materials);
    Result<CutsceneMaterials> tracks = sprites.and_then([&](const CutsceneSprites&) {
        return LoadWorldAnimations(vfs, folder, worlds)
            .and_then([&](const WorldAnimations& animations) {
                return CutsceneMaterials::Create(frame, models, animations, materials);
            });
    });
    if (!tracks) {
        return std::unexpected(std::move(tracks.error()));
    }
    return CutsceneLooks{.materials = std::move(*tracks), .sprites = std::move(*sprites)};
}

}

bool IsCutsceneScene(std::string_view scene) {
    return scene.starts_with(kCutsceneScenePrefix);
}

Result<std::unique_ptr<CutsceneScene>> CutsceneScene::Create(render::Renderer& renderer,
                                                             const assets::Vfs& vfs,
                                                             assets::TrackLoader& loader,
                                                             std::string_view scene) {
    Result<CutscenePath> path = ParsePath(scene);
    Result<std::unique_ptr<assets::CutsceneTimelineAsset>> timeline =
        path.and_then([&](const CutscenePath& found) { return ReadTimeline(vfs, found); });
    if (!timeline) {
        return std::unexpected(std::move(timeline.error()));
    }
    const std::vector<std::string>& worldNames = (*timeline)->worlds;
    Result<assets::TrackAssets> assets = loader.LoadWorlds(path->folder, worldNames);
    Result<TrackModel> model = assets.and_then(
        [&](const assets::TrackAssets& loaded) { return UploadTrack(renderer, loaded); });
    Result<sim::TrackEntitySources> sources = model.and_then(
        [&](const TrackModel&) { return sim::LoadWorldEntities(vfs, path->folder, worldNames); });
    const assets::CutsceneFrame& frame = (*timeline)->frames.front();
    const std::vector<const assets::WorldAsset*> worlds =
        assets ? WorldsOf(*assets) : std::vector<const assets::WorldAsset*>{};
    Result<CutsceneBindings> bindings =
        sources.and_then([&](const sim::TrackEntitySources& loaded) {
            return BindCutscene(frame, worlds, model->entityDraws, loaded);
        });
    Result<CutsceneLooks> looks = bindings.and_then([&](const CutsceneBindings& bound) {
        return LoadLooks(vfs, path->folder, worlds, frame, bound.models, model->materials);
    });
    if (!looks) {
        return Fail(looks.error().code, std::format("{}: {}", scene, looks.error().message));
    }
    auto owned = std::make_unique<sim::TrackEntitySources>(std::move(*sources));
    TrackEntityDraws jointed(model->animatedModels, *owned);
    CutscenePlayer player(frame, std::move(*bindings), std::move(*looks), *owned,
                          {.draws = model->draws, .slots = model->drawSlots}, true);
    log::Info("{}: {} worlds, {} frames, {} models", scene, worldNames.size(),
              (*timeline)->frames.size(), frame.models.size());
    return std::unique_ptr<CutsceneScene>(new CutsceneScene({.renderer = &renderer,
                                                             .name = std::string(scene),
                                                             .timeline = std::move(*timeline),
                                                             .sources = std::move(owned),
                                                             .statics = std::move(model->draws),
                                                             .slots = std::move(model->drawSlots),
                                                             .jointed = std::move(jointed),
                                                             .player = std::move(player)}));
}

void CutsceneScene::Tick([[maybe_unused]] const sim::InputFrame& input) {
    parts_.player.Advance(kSimTick);
}

void CutsceneScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entityClocks.assign(parts_.player.Clocks().begin(), parts_.player.Clocks().end());
}

void CutsceneScene::Render(const sim::RenderSnapshot& interpolated) {
    parts_.jointed.Update(interpolated.entityClocks);
    draws_.clear();
    parts_.player.AppendDraws(parts_.jointed, draws_);
    const render::Camera camera =
        parts_.player.ViewCamera(interpolated.entityClocks).value_or(render::Camera{});
    quads_.clear();
    parts_.player.AppendQuads(camera.transform, quads_);
    Result<void> drawn = parts_.renderer->Draw({.camera = camera,
                                                .lighting = parts_.player.Lighting(lights_),
                                                .meshes = draws_,
                                                .quads = quads_});
    if (!drawn) {
        log::Error("{}: {}", parts_.name, drawn.error().message);
    }
}

}
