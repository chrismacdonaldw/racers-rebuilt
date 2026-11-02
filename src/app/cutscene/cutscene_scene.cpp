#include "app/cutscene/cutscene_scene.hpp"

#include <format>
#include <optional>
#include <utility>

#include "app/cutscene/cutscene_bindings.hpp"
#include "app/cutscene/cutscene_materials.hpp"
#include "app/cutscene/cutscene_particles.hpp"
#include "app/cutscene/cutscene_racer.hpp"
#include "app/cutscene/cutscene_resources.hpp"
#include "app/cutscene/cutscene_screen.hpp"
#include "app/cutscene/cutscene_sounds.hpp"
#include "app/cutscene/cutscene_tints.hpp"
#include "app/frame_loop.hpp"
#include "app/track_model.hpp"
#include "assets/game_path.hpp"
#include "assets/racer_catalog.hpp"
#include "core/log.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

constexpr Milliseconds kSkipAfter{1000};

std::string NameOf(const CutscenePlay& play) {
    return std::format("{}/{}", play.folder, play.file);
}

render::Viewport ViewportOf(const std::optional<render::Rect>& rect) {
    const bool fillsCanvas =
        !rect || (rect->x == 0.0F && rect->y == 0.0F && rect->width == render::kUiWidth &&
                  rect->height == render::kUiHeight);
    if (fillsCanvas) {
        return {};
    }
    return {.x = rect->x / render::kUiWidth,
            .y = rect->y / render::kUiHeight,
            .width = rect->width / render::kUiWidth,
            .height = rect->height / render::kUiHeight,
            .isOnUiCanvas = true};
}

Result<std::unique_ptr<assets::CutsceneTimelineAsset>> ReadTimeline(const assets::Vfs& vfs,
                                                                    const CutscenePlay& path) {
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

Result<std::unique_ptr<assets::CutsceneEffectsAsset>> ReadEffects(const assets::Vfs& vfs,
                                                                  const CutscenePlay& path) {
    const std::string file = std::format("{}/{}.CEB", path.folder, path.file);
    Result<assets::FileData> data = vfs.Read(file);
    if (!data && data.error().code == ErrorCode::NotFound) {
        return std::make_unique<assets::CutsceneEffectsAsset>();
    }
    Result<assets::CutsceneEffectsAsset> effects = data.and_then(
        [](const assets::FileData& read) { return assets::ParseCutsceneEffects(read.Bytes()); });
    if (!effects) {
        return Fail(effects.error().code, std::format("{}: {}", file, effects.error().message));
    }
    return std::make_unique<assets::CutsceneEffectsAsset>(std::move(*effects));
}

void DropDriverLegVariants(const assets::Vfs& vfs, const CutscenePlay& play,
                           const assets::CutsceneFrame& frame, CutsceneBindings& bindings) {
    if (play.driver.empty()) {
        return;
    }
    const Result<assets::RacerFiles> racer = assets::RacerCatalog::Load(vfs).and_then(
        [&](const assets::RacerCatalog& catalog) { return catalog.Find(play.driver); });
    if (!racer) {
        log::Warn("{}, {} keeps its stand-in racer", racer.error().message, NameOf(play));
        return;
    }
    DropLegVariants(frame, bindings.models, racer->driver.legsIndex);
}

struct SceneWorld {
    std::span<const assets::WorldAsset* const> worlds;
    std::span<const NamedDraws> statics;
    const sim::TrackEntitySources* entities = nullptr;
    const assets::NameMap<assets::MaterialAsset>* materials = nullptr;
    const assets::NameMap<render::TextureId>* textures = nullptr;
};

// NOLINTNEXTLINE(bugprone-exception-escape)
struct EffectParts {
    UiFonts fonts;
    CutsceneEffects effects;
};

Result<EffectParts> LoadEffectParts(const RaceOutputs& outputs, const assets::Vfs& vfs,
                                    std::string_view folder, std::string_view language,
                                    const SceneWorld& world,
                                    const assets::CutsceneEffectsAsset& effects) {
    Result<CutsceneResources> resources =
        LoadCutsceneResources(*outputs.renderer, vfs, folder, language, effects);
    Result<std::vector<std::optional<audio::SoundId>>> sounds =
        resources.and_then([&](const auto&) {
            return LoadCutsceneSounds(*outputs.mixer, *outputs.banks, vfs, folder, effects);
        });
    Result<std::vector<std::optional<ModelBinding>>> tints = sounds.and_then([&](const auto&) {
        return BindTints(effects.tints, world.worlds, world.statics, *world.entities);
    });
    Result<std::vector<render::ParticleBank>> banks = tints.and_then([&](const auto&) {
        return LoadCutsceneParticles(vfs, folder, effects, *world.materials, *world.textures);
    });
    if (!banks) {
        return std::unexpected(std::move(banks.error()));
    }
    return EffectParts{
        .fonts = std::move(resources->fonts),
        .effects = {.sounds = CutsceneSounds(*outputs.mixer, effects.sounds, std::move(*sounds)),
                    .overlay = CutsceneOverlay(effects, std::move(resources->overlay)),
                    .tints = CutsceneTints(effects.tints, std::move(*tints)),
                    .particles = CutsceneParticles(effects.particles, std::move(*banks))}};
}

// NOLINTNEXTLINE(bugprone-exception-escape)
struct SceneEffects {
    std::unique_ptr<assets::CutsceneEffectsAsset> asset;
    UiFonts fonts;
    CutsceneEffectPlayer player;
};

Result<SceneEffects> LoadEffects(const RaceOutputs& outputs, const assets::Vfs& vfs,
                                 const CutscenePlay& path, std::string_view language,
                                 const assets::CutsceneFrame& frame, const SceneWorld& world) {
    Result<std::unique_ptr<assets::CutsceneEffectsAsset>> asset = ReadEffects(vfs, path);
    if (asset) {
        HideTexts(**asset, path.hiddenTexts);
    }
    Result<EffectParts> parts = asset.and_then([&](const auto& effects) {
        return LoadEffectParts(outputs, vfs, path.folder, language, world, *effects);
    });
    if (!parts) {
        return std::unexpected(std::move(parts.error()));
    }
    CutsceneEffectPlayer player(frame, **asset, std::move(parts->effects));
    return SceneEffects{
        .asset = std::move(*asset), .fonts = std::move(parts->fonts), .player = std::move(player)};
}

}

bool IsCutsceneScene(std::string_view scene) {
    return scene.starts_with(kCutsceneScenePrefix) || IsCutsceneScreen(scene);
}

Result<CutscenePlay> ViewerPlay(std::string_view scene) {
    const std::string_view path = scene.substr(kCutsceneScenePrefix.size());
    const std::size_t slash = path.find('/');
    if (slash == std::string_view::npos || slash == 0 || slash + 1 == path.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} isn't {}<folder>/<file>", scene, kCutsceneScenePrefix));
    }
    return CutscenePlay{
        .folder = std::format("MENUDATA/{}", assets::ToUpperAscii(path.substr(0, slash))),
        .file = assets::ToUpperAscii(path.substr(slash + 1))};
}

Result<std::unique_ptr<CutsceneScene>> CutsceneScene::Create(const RaceOutputs& outputs,
                                                             const assets::Vfs& vfs,
                                                             assets::TrackLoader& loader,
                                                             std::string_view language,
                                                             std::string_view scene) {
    return (IsCutsceneScreen(scene) ? ScreenPlay(vfs, scene) : ViewerPlay(scene))
        .and_then([&](CutscenePlay play) {
            return Create(outputs, vfs, loader, language, std::move(play));
        });
}

Result<std::unique_ptr<CutsceneScene>> CutsceneScene::Create(const RaceOutputs& outputs,
                                                             const assets::Vfs& vfs,
                                                             assets::TrackLoader& loader,
                                                             std::string_view language,
                                                             CutscenePlay play) {
    render::Renderer& renderer = *outputs.renderer;
    const std::string name = NameOf(play);
    Result<std::unique_ptr<assets::CutsceneTimelineAsset>> timeline = ReadTimeline(vfs, play);
    if (!timeline) {
        return std::unexpected(std::move(timeline.error()));
    }
    const std::vector<std::string>& worldNames = (*timeline)->worlds;
    Result<assets::TrackAssets> assets = loader.LoadWorlds(play.folder, worldNames);
    Result<TrackModel> model = assets.and_then(
        [&](const assets::TrackAssets& loaded) { return UploadTrack(renderer, loaded); });
    Result<sim::TrackEntitySources> sources = model.and_then(
        [&](const TrackModel&) { return sim::LoadWorldEntities(vfs, play.folder, worldNames); });
    const assets::CutsceneFrame& frame = (*timeline)->frames.front();
    const std::vector<const assets::WorldAsset*> worlds =
        assets ? WorldsOf(*assets) : std::vector<const assets::WorldAsset*>{};
    Result<CutsceneBindings> bindings =
        sources.and_then([&](const sim::TrackEntitySources& loaded) {
            return BindCutscene(frame, worlds, model->entityDraws, loaded);
        });
    if (bindings) {
        DropDriverLegVariants(vfs, play, frame, *bindings);
    }
    Result<CutsceneLooks> looks = bindings.and_then([&](const CutsceneBindings& bound) {
        return LoadLooks(vfs, play.folder, worlds, frame, bound.models, model->materials);
    });
    Result<SceneEffects> effects = looks.and_then([&](const CutsceneLooks&) {
        return LoadEffects(outputs, vfs, play, language, frame,
                           {.worlds = worlds,
                            .statics = model->entityDraws,
                            .entities = &*sources,
                            .materials = &assets->materials,
                            .textures = &model->textures});
    });
    if (!effects) {
        return Fail(effects.error().code, std::format("{}: {}", name, effects.error().message));
    }
    auto owned = std::make_unique<sim::TrackEntitySources>(std::move(*sources));
    TrackEntityDraws jointed(model->animatedModels, *owned);
    CutscenePlayer player(frame, std::move(*bindings), std::move(*looks), *owned,
                          {.draws = model->draws, .slots = model->drawSlots}, play.isLooping);
    log::Info("{}: {} worlds, {} frames, {} models", name, worldNames.size(),
              (*timeline)->frames.size(), frame.models.size());
    return std::unique_ptr<CutsceneScene>(
        new CutsceneScene({.renderer = &renderer,
                           .play = std::move(play),
                           .timeline = std::move(*timeline),
                           .sources = std::move(owned),
                           .statics = std::move(model->draws),
                           .slots = std::move(model->drawSlots),
                           .jointed = std::move(jointed),
                           .player = std::move(player),
                           .effects = std::move(effects->asset),
                           .fonts = std::move(effects->fonts),
                           .effectPlayer = std::move(effects->player)}));
}

CutsceneScene::~CutsceneScene() {
    parts_.effectPlayer.StopAll();
}

std::optional<std::string> CutsceneScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

void CutsceneScene::HandleEvent(const SDL_Event& event) {
    isSkipAsked_ = isSkipAsked_ || IsSkipPress(event);
}

void CutsceneScene::Tick([[maybe_unused]] const sim::InputFrame& input) {
    const bool isSkipped =
        std::exchange(isSkipAsked_, false) && parts_.play.isSkippable && played_ >= kSkipAfter;
    if (hasEnded_) {
        return;
    }
    if (isSkipped) {
        End();
        return;
    }
    parts_.effectPlayer.Advance(parts_.player.Advance(kSimTick), kSimTick);
    played_ += kSimTick;
    if (!parts_.play.isLooping && parts_.player.IsComplete()) {
        End();
    }
}

void CutsceneScene::End() {
    hasEnded_ = true;
    if (!parts_.play.next.empty()) {
        next_ = parts_.play.next;
    }
}

void CutsceneScene::Snapshot(sim::RenderSnapshot& out) const {
    out.entityClocks.assign(parts_.player.Clocks().begin(), parts_.player.Clocks().end());
}

void CutsceneScene::Render(const sim::RenderSnapshot& interpolated) {
    parts_.jointed.Update(interpolated.entityClocks);
    draws_.clear();
    parts_.player.AppendDraws(parts_.jointed, parts_.effectPlayer.Tints(), draws_);
    const render::Camera camera =
        parts_.player.ViewCamera(interpolated.entityClocks).value_or(render::Camera{});
    quads_.clear();
    parts_.player.AppendQuads(camera.transform, quads_);
    parts_.effectPlayer.AppendQuads(camera.transform, quads_);
    sprites_.clear();
    parts_.effectPlayer.Draw(parts_.play.rect.value_or(render::Rect{.width = render::kUiWidth,
                                                                    .height = render::kUiHeight}),
                             sprites_);
    Result<void> drawn = parts_.renderer->Draw({.camera = camera,
                                                .lighting = parts_.player.Lighting(lights_),
                                                .meshes = draws_,
                                                .quads = quads_,
                                                .sprites = sprites_,
                                                .viewport = ViewportOf(parts_.play.rect)});
    if (!drawn) {
        log::Error("{}: {}", NameOf(parts_.play), drawn.error().message);
    }
}

}
