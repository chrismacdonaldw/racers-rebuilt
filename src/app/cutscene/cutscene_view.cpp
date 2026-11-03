#include "app/cutscene/cutscene_view.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <utility>

#include "app/cutscene/cutscene_bindings.hpp"
#include "app/cutscene/cutscene_particles.hpp"
#include "app/cutscene/cutscene_racer.hpp"
#include "app/cutscene/cutscene_resources.hpp"
#include "app/cutscene/cutscene_sounds.hpp"
#include "app/cutscene/cutscene_tints.hpp"
#include "assets/game_path.hpp"
#include "assets/racer_catalog.hpp"
#include "core/log.hpp"
#include "render/ui_layout.hpp"

namespace rr::app {
namespace {

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

struct FrameSources {
    const assets::Vfs* vfs = nullptr;
    const CutscenePlay* play = nullptr;
    const assets::TrackAssets* assets = nullptr;
    const TrackModel* model = nullptr;
    const WorldAnimations* animations = nullptr;
    sim::TrackEntitySources* entities = nullptr;
};

Result<CutscenePlayer> PlayFrame(const FrameSources& from, const assets::CutsceneFrame& frame) {
    const std::vector<const assets::WorldAsset*> worlds = WorldsOf(*from.assets);
    Result<CutsceneBindings> bindings =
        BindCutscene(frame, worlds, from.model->entityDraws, *from.entities);
    if (bindings) {
        DropDriverLegVariants(*from.vfs, *from.play, frame, *bindings);
    }
    Result<CutsceneSprites> sprites = bindings.and_then([&](const CutsceneBindings& bound) {
        return CutsceneSprites::Create(bound.models, from.model->materials);
    });
    Result<CutsceneMaterials> materials = sprites.and_then([&](const CutsceneSprites&) {
        return CutsceneMaterials::Create(frame, bindings->models, *from.animations,
                                         from.model->materials);
    });
    if (!materials) {
        return std::unexpected(std::move(materials.error()));
    }
    return CutscenePlayer(
        frame, std::move(*bindings),
        {.materials = std::move(*materials), .sprites = std::move(*sprites)}, *from.entities,
        {.draws = from.model->draws, .slots = from.model->drawSlots}, from.play->isLooping);
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

// NOLINTNEXTLINE(bugprone-exception-escape)
struct LoadedWorlds {
    assets::TrackAssets assets;
    std::unique_ptr<TrackModel> model;
    WorldAnimations animations;
    std::unique_ptr<sim::TrackEntitySources> sources;
};

Result<LoadedWorlds> LoadWorlds(render::Renderer& renderer, const assets::Vfs& vfs,
                                assets::TrackLoader& loader, std::string_view folder,
                                std::span<const std::string> names) {
    Result<assets::TrackAssets> assets = loader.LoadWorlds(folder, names);
    Result<TrackModel> model = assets.and_then(
        [&](const assets::TrackAssets& loaded) { return UploadTrack(renderer, loaded); });
    Result<WorldAnimations> animations = model.and_then(
        [&](const TrackModel&) { return LoadWorldAnimations(vfs, folder, WorldsOf(*assets)); });
    Result<sim::TrackEntitySources> sources = animations.and_then(
        [&](const WorldAnimations&) { return sim::LoadWorldEntities(vfs, folder, names); });
    if (!sources) {
        return std::unexpected(std::move(sources.error()));
    }
    return LoadedWorlds{.assets = std::move(*assets),
                        .model = std::make_unique<TrackModel>(std::move(*model)),
                        .animations = std::move(*animations),
                        .sources = std::make_unique<sim::TrackEntitySources>(std::move(*sources))};
}

}

Result<std::unique_ptr<CutsceneView>> CutsceneView::Create(const RaceOutputs& outputs,
                                                           const assets::Vfs& vfs,
                                                           assets::TrackLoader& loader,
                                                           std::string_view language,
                                                           CutscenePlay play) {
    const std::string name = NameOf(play);
    Result<std::unique_ptr<assets::CutsceneTimelineAsset>> timeline = ReadTimeline(vfs, play);
    if (!timeline) {
        return std::unexpected(std::move(timeline.error()));
    }
    const assets::CutsceneFrame& frame = (*timeline)->frames.front();
    Result<LoadedWorlds> loaded =
        LoadWorlds(*outputs.renderer, vfs, loader, play.folder, (*timeline)->worlds);
    Result<CutscenePlayer> player = loaded.and_then([&](const LoadedWorlds& world) {
        return PlayFrame({.vfs = &vfs,
                          .play = &play,
                          .assets = &world.assets,
                          .model = world.model.get(),
                          .animations = &world.animations,
                          .entities = world.sources.get()},
                         frame);
    });
    const std::vector<const assets::WorldAsset*> worlds =
        loaded ? WorldsOf(loaded->assets) : std::vector<const assets::WorldAsset*>{};
    Result<SceneEffects> effects = player.and_then([&](const CutscenePlayer&) {
        return LoadEffects(outputs, vfs, play, language, frame,
                           {.worlds = worlds,
                            .statics = loaded->model->entityDraws,
                            .entities = loaded->sources.get(),
                            .materials = &loaded->assets.materials,
                            .textures = &loaded->model->textures});
    });
    if (!effects) {
        return Fail(effects.error().code, std::format("{}: {}", name, effects.error().message));
    }
    TrackEntityDraws jointed(loaded->model->animatedModels, *loaded->sources);
    log::Info("{}: {} worlds, {} frames, {} models", name, worlds.size(),
              (*timeline)->frames.size(), frame.models.size());
    return std::unique_ptr<CutsceneView>(
        new CutsceneView({.vfs = &vfs,
                          .play = std::move(play),
                          .timeline = std::move(*timeline),
                          .assets = std::move(loaded->assets),
                          .model = std::move(loaded->model),
                          .animations = std::move(loaded->animations),
                          .sources = std::move(loaded->sources),
                          .jointed = std::move(jointed),
                          .player = std::move(*player),
                          .effects = std::move(effects->asset),
                          .fonts = std::move(effects->fonts),
                          .effectPlayer = std::move(effects->player)}));
}

CutsceneView::~CutsceneView() {
    parts_.effectPlayer.StopAll();
}

Result<bool> CutsceneView::SelectFrame(std::string_view name) {
    const std::string wanted = assets::ToUpperAscii(name);
    const std::vector<assets::CutsceneFrame>& frames = parts_.timeline->frames;
    const auto found = std::ranges::find_if(frames, [&](const assets::CutsceneFrame& frame) {
        return assets::ToUpperAscii(frame.name) == wanted;
    });
    const auto index = static_cast<std::size_t>(found - frames.begin());
    if (found == frames.end() || index == parts_.frame) {
        return false;
    }
    Result<CutscenePlayer> player = PlayFrame({.vfs = parts_.vfs,
                                               .play = &parts_.play,
                                               .assets = &parts_.assets,
                                               .model = parts_.model.get(),
                                               .animations = &parts_.animations,
                                               .entities = parts_.sources.get()},
                                              *found);
    if (!player) {
        return Fail(player.error().code, std::format("{} frame {}: {}", NameOf(parts_.play),
                                                     found->name, player.error().message));
    }
    parts_.player = std::move(*player);
    parts_.effectPlayer.SetFrame(*found);
    parts_.frame = index;
    return true;
}

void CutsceneView::Advance(Milliseconds elapsed) {
    parts_.effectPlayer.Advance(parts_.player.Advance(elapsed), elapsed);
}

render::RenderView CutsceneView::Render(std::span<const sim::EntityClock> clocks,
                                        std::vector<render::SpriteDraw>& sprites) {
    parts_.jointed.Update(clocks);
    draws_.clear();
    parts_.player.AppendDraws(parts_.jointed, parts_.effectPlayer.Tints(), draws_);
    const render::Camera camera = parts_.player.ViewCamera(clocks).value_or(render::Camera{});
    quads_.clear();
    parts_.player.AppendQuads(camera.transform, quads_);
    parts_.effectPlayer.AppendQuads(camera.transform, quads_);
    parts_.effectPlayer.Draw(parts_.play.rect.value_or(render::Rect{.width = render::kUiWidth,
                                                                    .height = render::kUiHeight}),
                             sprites);
    return {.camera = camera,
            .lighting = parts_.player.Lighting(lights_),
            .meshes = draws_,
            .quads = quads_,
            .viewport = ViewportOf(parts_.play.rect)};
}

}
