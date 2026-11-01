#include "app/race_scene.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <system_error>
#include <utility>

#include "app/checkpoint_gates.hpp"
#include "app/collision_wireframe.hpp"
#include "app/frame_loop.hpp"
#include "app/game_settings.hpp"
#include "app/hud/race_hud_values.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/powerup_tools.hpp"
#include "app/race_step.hpp"
#include "app/racer/racer_fx.hpp"
#include "app/track_model.hpp"
#include "app/track_space.hpp"
#include "assets/circuit_database.hpp"
#include "core/log.hpp"
#include "platform/clock.hpp"
#include "sim/powerup/every_effect.hpp"

namespace rr::app {
namespace {

constexpr std::uint64_t kHashPrime = 1099511628211ULL;

sim::RaceSetup FullGrid(const RaceSceneSetup& scene, const RacerGrid& racers, std::uint64_t seed,
                        std::size_t slots) {
    sim::RaceSetup setup{.track = scene.track,
                         .seed = seed,
                         .respawn = scene.respawn,
                         .aiDriving = scene.aiDriving,
                         .effects = sim::EveryEffect()};
    setup.racers.resize(slots);
    for (std::size_t i = 0; i < slots; ++i) {
        setup.racers[i].chassis = racers.Chassis(i);
        setup.racers[i].localBounds = racers.LocalBounds(i);
        setup.racers[i].habits = racers.Habits(i);
    }
    setup.racers[kPlayerSlot].isPlayer = true;
    return setup;
}

std::vector<std::int32_t> PerSlot(const RacerGrid& racers, std::size_t slots,
                                  std::int32_t (RacerGrid::*value)(std::size_t) const) {
    std::vector<std::int32_t> values(slots);
    for (std::size_t i = 0; i < slots; ++i) {
        values[i] = (racers.*value)(i);
    }
    return values;
}

std::vector<sim::ChassisSpec> ChassisPerSlot(const RacerGrid& racers) {
    std::vector<sim::ChassisSpec> chassis(racers.Count());
    for (std::size_t i = 0; i < chassis.size(); ++i) {
        chassis[i] = racers.Chassis(i);
    }
    return chassis;
}

std::unique_ptr<RaceAudio> LoadAudio(const RaceOutputs& outputs, const assets::Vfs& vfs,
                                     std::string_view track, const RaceAudioSetup& setup) {
    Result<std::unique_ptr<RaceAudio>> audio =
        RaceAudio::Load(*outputs.mixer, *outputs.banks, vfs, track, setup);
    if (!audio) {
        log::Warn("{}, racing in silence", audio.error().message);
        return nullptr;
    }
    return std::move(*audio);
}

float MsSince(std::optional<Nanoseconds>& last) {
    const Nanoseconds now = platform::Now();
    const Nanoseconds elapsed = last ? now - *last : Nanoseconds{};
    last = now;
    return std::chrono::duration<float, std::milli>(elapsed).count();
}

std::optional<RaceParticles> LoadParticles(render::Renderer& renderer, const assets::Vfs& vfs,
                                           assets::TrackLoader& loader, std::string_view track,
                                           std::span<const assets::SurfaceEntry> surfaces) {
    Result<RaceParticles> particles =
        RaceParticles::Load(renderer, vfs, loader, track, surfaces, kPlayerSlot);
    if (!particles) {
        log::Warn("{}, racing without particles", particles.error().message);
        return std::nullopt;
    }
    return std::move(*particles);
}

}

Result<std::unique_ptr<RaceScene>> RaceScene::Create(const RaceOutputs& outputs,
                                                     const assets::Vfs& vfs,
                                                     assets::TrackLoader& loader,
                                                     const RaceSceneSetup& setup,
                                                     std::uint64_t seed, Nanoseconds started) {
    render::Renderer& renderer = *outputs.renderer;
    const std::string& track = setup.track;
    Result<TrackModel> model = loader.Load(std::format("GAMEDATA/{}", track))
                                   .and_then([&](const assets::TrackAssets& assets) {
                                       return UploadTrack(renderer, assets);
                                   });
    Result<sim::RaceTrack> loaded =
        model.and_then([&](const TrackModel&) { return sim::LoadRaceTrack(vfs, track); });
    Result<RacerGrid> racers = loaded.and_then([&](const sim::RaceTrack& race) {
        return RacerGrid::Load(renderer, vfs, loader, setup, race.grid.size());
    });
    Result<BrickModels> bricks =
        racers.and_then([&](const RacerGrid&) { return BrickModels::Load(renderer, loader); });
    if (!bricks) {
        return std::unexpected(std::move(bricks.error()));
    }
    auto raceTrack = std::make_unique<sim::RaceTrack>(std::move(*loaded));
    auto world = std::make_unique<sim::WorldList>(raceTrack->collision.Track());
    TrackEntityDraws entityDraws(model->animatedModels, raceTrack->entities);
    Result<sim::RaceSession> session = sim::RaceSession::Create(
        FullGrid(setup, *racers, seed, racers->Count()), sim::CourseOf(*raceTrack, *world));
    if (!session) {
        return std::unexpected(std::move(session.error()));
    }
    HiddenModels hiddenModels(raceTrack->events.table.modelDistances,
                              session->TrackHazards().Models().Names(), model->entityDraws,
                              raceTrack->entities);
    GhostTrail ghostTrail(model->draws, model->entityDraws, raceTrack->entities);
    WaterScroll waterScroll(raceTrack->hazards, model->entityDraws);
    std::unique_ptr<RaceAudio> audio = LoadAudio(
        outputs, vfs, track,
        {.racerCount = session->Racers().size(),
         .player = kPlayerSlot,
         .seed = seed,
         .ambience = raceTrack->events.table.sounds,
         .voiceBanks = PerSlot(*racers, session->Racers().size(), &RacerGrid::VoiceBank)});
    std::optional<RaceParticles> particles =
        LoadParticles(renderer, vfs, loader, track, raceTrack->collision.surfaces);
    return std::unique_ptr<RaceScene>(
        new RaceScene({.renderer = &renderer,
                       .vfs = &vfs,
                       .setup = setup,
                       .trackDraws = std::move(model->draws),
                       .racers = std::move(*racers),
                       .racerFx = LoadRacerFxMaterials(renderer, loader),
                       .bricks = std::move(*bricks),
                       .powerups = LoadPowerupModels(renderer, vfs, loader),
                       .warpTube = WarpTube::Load(renderer, vfs, loader),
                       .camera = TrackCamera(model->bounds),
                       .raceTrack = std::move(raceTrack),
                       .entityDraws = std::move(entityDraws),
                       .hiddenModels = std::move(hiddenModels),
                       .ghostTrail = std::move(ghostTrail),
                       .waterScroll = std::move(waterScroll),
                       .world = std::move(world),
                       .session = std::move(*session),
                       .audio = std::move(audio),
                       .particles = std::move(particles),
                       .started = started}));
}

RaceScene::RaceScene(Parts parts)
    : parts_(std::move(parts)),
      shadows_(parts_.racerFx, ChassisPerSlot(parts_.racers)),
      powerupVisuals_(parts_.powerups.models),
      powerupQuads_(parts_.powerups.quadMaterials),
      chase_(PlayerCar()),
      hud_(*parts_.renderer, *parts_.vfs, kPlayerSlot,
           PerSlot(parts_.racers, parts_.session.Racers().size(), &RacerGrid::NameIndex)),
      controls_(parts_.session.Racers().size()),
      collisionLines_(BuildCollisionWireframe(parts_.raceTrack->collision)),
      triggerRings_(parts_.session.Triggers()) {
    draws_ = parts_.trackDraws;
    draws_.insert(draws_.end(), parts_.racers.Draws().begin(), parts_.racers.Draws().end());
}

void RaceScene::ApplySettings(const settings::Registry& settings) {
    showsCheckpoints_ = settings.Get(kShowCheckpoints);
    showsCollision_ = settings.Get(kCollisionWireframe);
    showsTriggers_ = settings.Get(kShowTriggerSpheres);
    hud_.SetLanguage(SelectedLanguage(settings));
}

std::optional<std::string> RaceScene::TakeNextScene() {
    return std::exchange(next_, std::nullopt);
}

overlay::RacerTools RaceScene::OverlayRacerTools() {
    overlay::RacerTools tools{.selected = &chosenRacer_};
    const std::span<const sim::Racer> racers = std::as_const(parts_.session).Racers();
    for (std::size_t i = 0; i < racers.size(); ++i) {
        tools.racers.push_back(i == kPlayerSlot ? std::format("Racer {} (you)", i + 1)
                                                : std::format("Racer {}", i + 1));
    }
    tools.groups.push_back(EffectTools());
    tools.groups.push_back(PowerupTools());
    return tools;
}

overlay::RacerActionGroup RaceScene::EffectTools() {
    overlay::RacerActionGroup group{.name = "Effects"};
    const sim::Racer& racer = std::as_const(parts_.session).Racers()[chosenRacer_];
    for (const RacerEffect effect : kRacerEffects) {
        group.actions.push_back({.name = RacerEffectLabel(effect, racer), .run = [this, effect] {
                                     chosenTools_.emplace_back(
                                         [racer = chosenRacer_, effect](sim::RaceSession& race) {
                                             ApplyRacerEffect(effect, race.Racers()[racer]);
                                         });
                                 }});
    }
    return group;
}

overlay::RacerActionGroup RaceScene::PowerupTools() {
    overlay::RacerActionGroup group{.name = "Give power-up"};
    for (const assets::BrickColour colour : kBrickColours) {
        for (std::uint8_t level = 0; level <= sim::kMaxPowerupLevel; ++level) {
            group.actions.push_back(
                {.name = PowerupLabel(colour, level), .run = [this, colour, level] {
                     chosenTools_.emplace_back(
                         [racer = chosenRacer_, colour, level](sim::RaceSession& race) {
                             race.GivePowerup(racer, colour, level);
                         });
                 }});
        }
    }
    return group;
}

void RaceScene::Tick(const sim::InputFrame& input) {
    if (pause_.IsOpen() || input.WasPressed(sim::Action::Pause)) {
        TickPause(input);
        return;
    }
    ApplyChosenTools();
    if (ShowsResults(parts_.session.Clock()) && input.WasPressed(sim::MenuAction::Confirm)) {
        next_ = std::string(kMainMenuScene);
    }
    StepRace(parts_.session, input, controls_);
    hud_.Tick(parts_.session, kSimTick);
    parts_.waterScroll.Tick(kSimTick);
    if (parts_.session.WasRespawned(kPlayerSlot)) {
        chase_.JumpTo(PlayerCar());
    } else {
        chase_.Follow(PlayerCar());
    }
    if (parts_.audio) {
        parts_.audio->Hear(parts_.session, chase_.Pose(PlayerCar()), kSimTick);
    }
    if (parts_.particles) {
        parts_.particles->Tick(parts_.session, chase_.Pose(PlayerCar()), kSimTick);
    }
}

SceneMusic RaceScene::Music() const {
    return parts_.audio ? parts_.audio->Music() : SceneMusic{};
}

void RaceScene::Snapshot(sim::RenderSnapshot& out) const {
    SnapshotRace(parts_.session, out);
    out.camera = {.transform = chase_.Pose(PlayerCar()), .flags = out.entities[kPlayerSlot].flags};
    if (pause_.IsOpen()) {
        out.simStateHash = (out.simStateHash * kHashPrime) ^ pause_.StateHash();
    }
}

void RaceScene::GatherMeshes(const sim::RenderSnapshot& interpolated, float elapsedMs) {
    carLooks_.resize(parts_.racers.Count());
    LookCars(interpolated, parts_.raceTrack->events.table.colorTransforms, carLooks_);
    parts_.racers.Update(interpolated.entities, interpolated.racers, carLooks_, elapsedMs);
    draws_.resize(parts_.trackDraws.size() + parts_.racers.Draws().size());
    std::ranges::copy(parts_.racers.Draws(),
                      draws_.begin() + static_cast<std::ptrdiff_t>(parts_.trackDraws.size()));
    parts_.entityDraws.Update(interpolated.entityClocks);
    draws_.insert(draws_.end(), parts_.entityDraws.Draws().begin(),
                  parts_.entityDraws.Draws().end());
    parts_.hiddenModels.Apply(interpolated.hiddenModels, parts_.trackDraws, parts_.entityDraws,
                              parts_.trackDraws.size() + parts_.racers.Draws().size(), draws_);
    parts_.ghostTrail.Append(parts_.session.TrackHazards(), interpolated.entityClocks, draws_);
    parts_.waterScroll.Apply(draws_);
    parts_.bricks.AppendDraws(interpolated.bricks, draws_);
    powerupVisuals_.Update(interpolated, elapsedMs);
    draws_.insert(draws_.end(), powerupVisuals_.Draws().begin(), powerupVisuals_.Draws().end());
}

void RaceScene::Render(const sim::RenderSnapshot& interpolated) {
    const float elapsedMs = MsSince(lastRender_);
    GatherMeshes(interpolated, elapsedMs);
    const sim::EffectSnapshot* start =
        FindWarp(interpolated.effects, kPlayerSlot, sim::WarpPhase::Starting);
    render::Camera camera = parts_.camera;
    camera.transform = interpolated.camera.transform;
    if (start != nullptr) {
        camera = WarpStartCamera(camera, start->phaseLeftMs);
    }
    DrawDebugLines();
    quads_.clear();
    shadows_.Append(interpolated.entities, interpolated.racers, carLooks_, *parts_.world, quads_);
    if (parts_.particles) {
        parts_.particles->Update(parts_.session,
                                 render::FloatMilliseconds{pause_.IsOpen() ? 0.0F : elapsedMs});
        parts_.particles->AppendQuads(camera.transform, quads_);
    }
    powerupQuads_.Update(interpolated, camera.transform, elapsedMs);
    quads_.insert(quads_.end(), powerupQuads_.Quads().begin(), powerupQuads_.Quads().end());
    render::RenderView view{
        .camera = camera,
        .meshes = draws_,
        .quads = quads_,
        .lines = debugLines_,
        .sprites = hud_.Draw(parts_.session, interpolated.racers[kPlayerSlot],
                             start != nullptr ? WarpFlash(start->phaseLeftMs) : std::nullopt,
                             PauseFocus())};
    const sim::EffectSnapshot* flight =
        FindWarp(interpolated.effects, kPlayerSlot, sim::WarpPhase::Active);
    if (flight != nullptr && parts_.warpTube) {
        parts_.warpTube->Fly(flight->phaseLeftMs, interpolated.entities[kPlayerSlot].transform,
                             parts_.racers.SlotDraws(kPlayerSlot));
        view = {.camera = WarpFlightCamera(parts_.camera, flight->phaseLeftMs),
                .meshes = parts_.warpTube->Draws(),
                .sprites = view.sprites};
    }
    Result<void> drawn = parts_.renderer->Draw(view);
    if (!drawn) {
        log::Error("{}", drawn.error().message);
        return;
    }
    if (!hasDrawn_) {
        hasDrawn_ = true;
        log::Info(
            "{}: race of {} ready, first frame {} ms after loading began", parts_.setup.track,
            parts_.session.Racers().size(),
            std::chrono::duration_cast<Milliseconds>(platform::Now() - parts_.started).count());
    }
}

Transform RaceScene::PlayerCar() const {
    return CarToEngine(parts_.session.Racers()[kPlayerSlot].Pose());
}

void RaceScene::TickPause(const sim::InputFrame& input) {
    const std::optional<PauseRow> chosen = pause_.Tick(input);
    if (parts_.audio) {
        parts_.audio->StopLoops();
    }
    if (chosen == PauseRow::Restart) {
        next_ = RaceSceneName(parts_.setup);
    } else if (chosen == PauseRow::Quit) {
        next_ = std::string(kMainMenuScene);
    }
}

void RaceScene::ApplyChosenTools() {
    for (const std::function<void(sim::RaceSession&)>& tool : chosenTools_) {
        tool(parts_.session);
    }
    chosenTools_.clear();
}

std::optional<std::size_t> RaceScene::PauseFocus() const {
    return pause_.IsOpen() ? std::optional(static_cast<std::size_t>(pause_.Focus())) : std::nullopt;
}

void RaceScene::DrawDebugLines() {
    debugLines_.clear();
    if (showsCheckpoints_) {
        DrawGates(parts_.raceTrack->checkpoints,
                  parts_.session.Progress().OfRacer(kPlayerSlot).laps, debugLines_);
    }
    if (showsCollision_) {
        debugLines_.insert(debugLines_.end(), collisionLines_.begin(), collisionLines_.end());
    }
    if (showsTriggers_) {
        triggerRings_.Light(parts_.session.Triggers());
        debugLines_.insert(debugLines_.end(), triggerRings_.Lines().begin(),
                           triggerRings_.Lines().end());
    }
}

}
