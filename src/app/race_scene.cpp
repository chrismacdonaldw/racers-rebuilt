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
#include "app/fix_settings.hpp"
#include "app/frame_loop.hpp"
#include "app/game_settings.hpp"
#include "app/hud/race_map_view.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/player_cheat_settings.hpp"
#include "app/race_scene_parts.hpp"
#include "app/race_step.hpp"
#include "app/racer/far_cars.hpp"
#include "app/racer/racer_fx.hpp"
#include "app/racer/slot_draws.hpp"
#include "app/texture_scroll.hpp"
#include "app/track_map.hpp"
#include "app/track_model.hpp"
#include "app/track_space.hpp"
#include "app/video_settings.hpp"
#include "assets/circuit_database.hpp"
#include "core/log.hpp"
#include "platform/clock.hpp"

namespace rr::app {
namespace {

constexpr std::uint64_t kHashPrime = 1099511628211ULL;
constexpr Milliseconds kCircuitResultsTime{5000};
constexpr double kFastForwardScale = 1.75;
constexpr double kSlowMotionScale = 0.5;

std::string MenuAfter(const RaceSceneSetup& setup) {
    return std::string(setup.isTestDrive ? kGarageScene : kMainMenuScene);
}

Result<TrackModel> LoadTrackModel(render::Renderer& renderer, assets::TrackLoader& loader,
                                  const RaceSceneSetup& setup) {
    return loader.Load(std::format("GAMEDATA/{}", setup.track), setup.isMirrored)
        .and_then([&](const assets::TrackAssets& assets) { return UploadTrack(renderer, assets); });
}

}

Result<std::unique_ptr<RaceScene>> RaceScene::Create(const RaceOutputs& outputs,
                                                     const assets::Vfs& vfs,
                                                     assets::TrackLoader& loader,
                                                     const RaceSceneSetup& setup,
                                                     std::uint64_t seed, Nanoseconds started) {
    render::Renderer& renderer = *outputs.renderer;
    Result<TrackModel> model = LoadTrackModel(renderer, loader, setup);
    Result<sim::RaceTrack> loaded =
        model.and_then([&](const TrackModel&) { return LoadSceneTrack(vfs, setup); });
    Result<RacerGrid> racers = loaded.and_then([&](const sim::RaceTrack& race) {
        return RacerGrid::Load(renderer, vfs, loader, setup, RacerCount(setup, race.grid.size()));
    });
    Result<BrickModels> bricks =
        racers.and_then([&](const RacerGrid&) { return BrickModels::Load(renderer, vfs, loader); });
    Result<SkyBackdrop> sky = bricks.and_then([&](const BrickModels&) {
        return SkyBackdrop::Upload(renderer, std::move(model->skyModels));
    });
    if (!sky) {
        return std::unexpected(std::move(sky.error()));
    }
    auto raceTrack = std::make_unique<sim::RaceTrack>(std::move(*loaded));
    const sim::RaceTrack& race = *raceTrack;
    auto world = std::make_unique<sim::WorldList>(race.collision.Track());
    Result<sim::RaceSession> session = sim::RaceSession::Create(
        GridSetup(vfs, setup, *racers, seed, racers->Count()), sim::CourseOf(*raceTrack, *world));
    if (!session) {
        return std::unexpected(std::move(session.error()));
    }
    GhostTrail ghostTrail(model->draws, model->entityDraws, raceTrack->entities);
    const RacerSounds racerSounds = RacerSoundsOf(*racers, session->Racers().size());
    return std::unique_ptr<RaceScene>(new RaceScene(
        {.renderer = &renderer,
         .vfs = &vfs,
         .setup = setup,
         .trackDraws = std::move(model->draws),
         .decalSurface = std::move(model->decalSurface),
         .racers = std::move(*racers),
         .racerFx = LoadRacerFxMaterials(renderer, loader),
         .bricks = std::move(*bricks),
         .powerups = LoadPowerupModels(renderer, vfs, loader),
         .warpTube = WarpTube::Load(renderer, vfs, loader),
         .sky = std::move(*sky),
         .camera = TrackCamera(model->bounds),
         .raceTrack = std::move(raceTrack),
         .entityDraws = TrackEntityDraws(model->animatedModels, race.entities),
         .farModels = FarModels(std::move(model->farModels)),
         .sprites = std::move(model->sprites),
         .materialAnims = std::move(model->materialAnims),
         .hiddenModels = HiddenModels(race.events.table.modelDistances,
                                      session->TrackHazards().Models().Names(), model->entityDraws,
                                      race.entities),
         .ghostTrail = std::move(ghostTrail),
         .craneShadow =
             CraneShadow(LoadCraneShadowLook(renderer, vfs, loader, setup.track), race.entities),
         .waterScroll = WaterScroll(race.hazards, model->entityDraws),
         .scrolledDraws = std::move(model->scrolledDraws),
         .world = std::move(world),
         .session = std::move(*session),
         .audio = LoadRaceAudio(outputs, vfs, setup.track,
                                AudioSetup(vfs, setup, race, racerSounds, seed)),
         .particles =
             LoadRaceParticles(renderer, vfs, loader, setup.track, race.collision.surfaces),
         .map = LoadTrackMap(vfs, setup.track, setup.isMirrored, model->textures),
         .started = started}));
}

RaceScene::RaceScene(Parts parts)
    : parts_(std::move(parts)),
      groundDecals_(parts_.decalSurface),
      shadows_(parts_.racerFx, ShadowCasters(*parts_.renderer, parts_.racers)),
      skids_(PlayerSkids(parts_.racerFx, parts_.racers, parts_.setup.players)),
      powerupVisuals_(parts_.powerups.models),
      powerupQuads_(PlayerPowerupQuads(parts_.powerups.quadMaterials, parts_.setup.players)),
      cameras_(parts_.session, parts_.setup.players,
               PlayerLens(parts_.camera, parts_.setup.players).verticalFovDegrees),
      hud_(*parts_.renderer, *parts_.vfs, parts_.setup.players,
           PerSlot(parts_.racers, parts_.session.Racers().size(), &RacerGrid::Name)),
      controls_(parts_.session.Racers().size()),
      collisionLines_(BuildCollisionWireframe(parts_.raceTrack->collision)),
      triggerRings_(parts_.session.Triggers()) {
    draws_ = parts_.trackDraws;
    draws_.insert(draws_.end(), parts_.racers.Draws().begin(), parts_.racers.Draws().end());
    parts_.entityDraws.AddFarModels(draws_.size(), parts_.farModels);
    parts_.materialAnims.FindEntities(parts_.raceTrack->entities, parts_.entityDraws);
    hud_.SetMap(parts_.map);
    if (parts_.setup.isDemo) {
        hud_.ShowDemo();
    }
}

void RaceScene::ApplySettings(const settings::Registry& settings) {
    showsCheckpoints_ = settings.Get(kShowCheckpoints);
    showsCollision_ = settings.Get(kCollisionWireframe);
    showsTriggers_ = settings.Get(kShowTriggerSpheres);
    hasSlowMotion_ = settings.Get(kSlowMotionCheat);
    parts_.farModels.SetHidden(!settings.Get(kFarCarsSetting), !settings.Get(kFarScenerySetting));
    showsCockpitCar_ = settings.Get(kCockpitCarSetting);
    if (parts_.particles) {
        parts_.particles->SetBetter(settings.Get(kBetterParticlesSetting));
    }
    hud_.UseTextFixes(TextFixesFrom(settings));
    hud_.SetLanguage(SelectedLanguage(settings));
}

void RaceScene::AdoptHudFonts(std::string_view language, UiFonts fonts) {
    hud_.SetLanguage(language, std::move(fonts));
}

void RaceScene::JoinCircuit(CircuitLeg leg) {
    hud_.SetCircuitPoints(leg.Points());
    circuit_ = std::move(leg);
}

void RaceScene::JoinTimeTrial(TimeTrialRun& run, assets::TrackLoader& loader) {
    trial_.emplace(run);
    hud_.SetTimeTrial(trial_->Ghosts());
    ghostCars_ = GhostCars::Load(*parts_.renderer, *parts_.vfs, loader, parts_.racers, kPlayerSlot);
}

void RaceScene::Tick(const sim::InputFrame& input) {
    if (parts_.setup.isDemo && input.WasAnythingPressed()) {
        next_ = std::string(kMainMenuScene);
        return;
    }
    if (trial_ && trial_->IsAsking(parts_.session.Clock())) {
        TickTrialQuestion(*trial_, input);
        return;
    }
    if (pause_.IsOpen() || input.WasAnyPressed(sim::Action::Pause)) {
        TickPause(input);
        return;
    }
    isSlowMotion_ =
        hasSlowMotion_ && !parts_.setup.isDemo && input.IsAnyHeld(sim::Action::SlowMotion);
    ApplyChosenTools();
    if (!trial_) {
        LeaveAfterResults();
    }
    StepRace(parts_.session, input, controls_, parts_.setup.isDemo ? 0 : cameras_.Count());
    if (trial_) {
        trial_->Follow(parts_.session, hud_.RacerName(kPlayerSlot));
    }
    for (std::size_t player = 0; player < cameras_.Count(); ++player) {
        const sim::PlayerInput& controls = input.players.at(player);
        if (controls.WasPressed(sim::Action::ToggleDisplay)) {
            hud_.CycleGadget(player);
        }
        if (controls.WasPressed(sim::Action::ChangeCamera)) {
            cameras_.CycleView(player, parts_.session);
        }
        cameras_.SetLookBack(player, controls.IsHeld(sim::Action::LookBack));
    }
    hud_.Tick(parts_.session, kSimTick);
    parts_.waterScroll.Tick(kSimTick);
    cameras_.Follow(parts_.session);
    if (parts_.audio) {
        parts_.audio->Hear(parts_.session, cameras_.Poses(), kSimTick);
    }
    if (parts_.particles) {
        parts_.particles->Tick(parts_.session, cameras_.Poses().front(), kSimTick);
    }
}

SceneMusic RaceScene::Music() const {
    return parts_.audio ? parts_.audio->Music() : SceneMusic{};
}

double RaceScene::TimeScale() const {
    const bool isRacing = !pause_.IsOpen() && !IsTrialAsking();
    const double fastForward =
        parts_.setup.cheats.Has(sim::Cheat::FastForward) ? kFastForwardScale : 1.0;
    const double slowMotion = isSlowMotion_ ? kSlowMotionScale : 1.0;
    return isRacing ? fastForward * slowMotion : 1.0;
}

void RaceScene::Snapshot(sim::RenderSnapshot& out) const {
    SnapshotRace(parts_.session, out);
    cameras_.Snapshot(out);
    out.ghosts = trial_ ? sim::GhostSnapshot{.best = trial_->Ghosts().BestFrame(),
                                             .record = trial_->Ghosts().RecordFrame()}
                        : sim::GhostSnapshot{};
    if (pause_.IsOpen()) {
        out.simStateHash = (out.simStateHash * kHashPrime) ^ pause_.StateHash();
    }
}

void RaceScene::GatherMeshes(const sim::RenderSnapshot& interpolated, float simMs) {
    parts_.farModels.Reset(draws_);
    carLooks_.resize(parts_.racers.Count());
    LookCars(interpolated, parts_.raceTrack->events.table.colorTransforms, carLooks_);
    parts_.racers.Update(interpolated.entities, interpolated.racers, carLooks_, simMs);
    draws_.resize(parts_.trackDraws.size() + parts_.racers.Draws().size());
    std::ranges::copy(parts_.racers.Draws(),
                      draws_.begin() + static_cast<std::ptrdiff_t>(parts_.trackDraws.size()));
    AddGridCars(parts_.racers, interpolated.entities, parts_.trackDraws.size(), parts_.farModels);
    parts_.entityDraws.Update(interpolated.entityClocks);
    parts_.entityDraws.Scroll(shownMs_);
    const std::size_t entityFirst = draws_.size();
    draws_.insert(draws_.end(), parts_.entityDraws.Draws().begin(),
                  parts_.entityDraws.Draws().end());
    parts_.materialAnims.Apply(shownMs_, interpolated.hazardMaterials, entityFirst, draws_);
    parts_.hiddenModels.Apply(interpolated.hiddenModels, parts_.trackDraws, parts_.entityDraws,
                              entityFirst, draws_);
    parts_.ghostTrail.Append(parts_.session.TrackHazards(), interpolated.entityClocks, draws_);
    ScrollDraws(parts_.scrolledDraws, shownMs_, draws_);
    parts_.waterScroll.Apply(draws_);
    parts_.bricks.AppendDraws(interpolated.bricks, draws_, parts_.farModels);
    powerupVisuals_.Update(interpolated, simMs);
    parts_.farModels.Add(FarKind::Car, powerupVisuals_.PlacedModels(), draws_.size());
    draws_.insert(draws_.end(), powerupVisuals_.Draws().begin(), powerupVisuals_.Draws().end());
    if (ghostCars_) {
        ghostCars_->Update(interpolated.ghosts, simMs);
        ghostCars_->AddFarCars(interpolated.ghosts, draws_.size(), parts_.farModels);
        ghostCars_->AppendDraws(draws_);
    }
}

std::span<const MapView> RaceScene::PlayerMapViews(const sim::RenderSnapshot& interpolated) {
    const std::size_t players = cameras_.Count();
    for (std::size_t player = 0; player < players; ++player) {
        mapViews_.at(player) =
            PlayerMapView(interpolated, kPlayerSlot + player, players,
                          cameras_.ViewDirection(player), mapMarkers_.at(player));
    }
    return std::span(mapViews_).first(players);
}

void RaceScene::GatherCarMarks(const sim::RenderSnapshot& interpolated, float simMs) {
    carMarks_.clear();
    std::array<Vector3, sim::kMaxPlayers> cameras{};
    for (std::size_t player = 0; player < cameras_.Count(); ++player) {
        cameras.at(player) = PlayerCameraPose(interpolated, player).position;
    }
    shadows_.Append(interpolated.entities, interpolated.racers, carLooks_,
                    std::span(cameras).first(cameras_.Count()), groundDecals_, carMarks_);
    for (std::size_t player = 0; player < skids_.size(); ++player) {
        const std::size_t slot = kPlayerSlot + player;
        skids_[player].Update(interpolated.entities[slot], interpolated.racers[slot], simMs);
        skids_[player].Append(groundDecals_, carMarks_);
        skidding_.at(player) = skids_[player].Skidding();
    }
    parts_.craneShadow.Append(
        parts_.session.TrackHazards(), interpolated.entityClocks,
        std::span(cameras).first(parts_.farModels.HidesScenery() ? cameras_.Count() : 0),
        groundDecals_, carMarks_);
}

render::RenderView RaceScene::PlayerView(const sim::RenderSnapshot& interpolated,
                                         std::size_t player, float simMs) {
    const std::size_t slot = kPlayerSlot + player;
    render::Camera lens = PlayerLens(parts_.camera, cameras_.Count());
    lens.verticalFovDegrees = cameras_.FovDegrees(player);
    const render::Viewport viewport = PlayerViewport(player, cameras_.Count());
    const bool hidesOwnCar = !showsCockpitCar_ && cameras_.HidesOwnCar(player, parts_.session);
    const sim::EffectSnapshot* flight =
        FindWarp(interpolated.effects, slot, sim::WarpPhase::Active);
    if (flight != nullptr && parts_.warpTube) {
        parts_.warpTube->Fly(
            flight->phaseLeftMs, interpolated.entities[slot].transform,
            hidesOwnCar ? std::span<const render::MeshDraw>{} : parts_.racers.SlotDraws(slot));
        std::vector<render::MeshDraw>& tube = tubeDraws_.at(player);
        tube.assign(parts_.warpTube->Draws().begin(), parts_.warpTube->Draws().end());
        return {.camera = WarpFlightCamera(lens, flight->phaseLeftMs),
                .meshes = tube,
                .viewport = viewport};
    }
    render::Camera camera = lens;
    camera.transform = PlayerCameraPose(interpolated, player);
    const sim::EffectSnapshot* start =
        FindWarp(interpolated.effects, slot, sim::WarpPhase::Starting);
    if (start != nullptr) {
        camera = WarpStartCamera(camera, start->phaseLeftMs);
    }
    std::vector<render::QuadDraw>& quads = playerQuads_.at(player);
    quads.assign(carMarks_.begin(), carMarks_.end());
    parts_.sprites.Append(camera.transform, parts_.farModels.HidesScenery(), shownMs_, quads);
    if (parts_.particles) {
        parts_.particles->AppendQuads(camera.transform, quads);
    }
    PowerupQuads& powerupQuads = powerupQuads_[player];
    powerupQuads.Update(interpolated, camera.transform, simMs);
    quads.insert(quads.end(), powerupQuads.Quads().begin(), powerupQuads.Quads().end());
    std::vector<render::MeshDraw>& sky = skyDraws_.at(player);
    parts_.sky.Place(parts_.session.Resources().Sky(), camera.transform.position, shownMs_, sky);
    std::span<const render::MeshDraw> meshes =
        parts_.farModels.Cull(player, camera.transform.position, draws_);
    if (hidesOwnCar) {
        meshes = WithoutDraws(meshes, SlotDrawSpan(parts_.racers, slot, parts_.trackDraws.size()),
                              ownCarHidden_.at(player));
    }
    return {.camera = camera,
            .backdrop = sky,
            .meshes = meshes,
            .quads = quads,
            .lines = debugLines_,
            .viewport = viewport};
}

void RaceScene::Render(const sim::RenderSnapshot& interpolated) {
    const float elapsedMs = MsSince(lastRender_) * static_cast<float>(TimeScale());
    const float simMs = pause_.IsOpen() || IsTrialAsking() ? 0.0F : elapsedMs;
    shownMs_ += simMs;
    GatherMeshes(interpolated, simMs);
    DrawDebugLines();
    GatherCarMarks(interpolated, simMs);
    if (parts_.particles) {
        parts_.particles->Update(parts_.session, std::span(skidding_).first(skids_.size()),
                                 render::FloatMilliseconds{simMs});
    }
    views_.clear();
    for (std::size_t player = 0; player < cameras_.Count(); ++player) {
        views_.push_back(PlayerView(interpolated, player, simMs));
    }
    const auto flashes = WarpFlashes(interpolated.effects, kPlayerSlot, cameras_.Count());
    views_.front().sprites =
        hud_.Draw(parts_.session, interpolated.racers, PlayerMapViews(interpolated),
                  std::span(flashes).first(cameras_.Count()), PauseShown());
    Result<void> drawn = parts_.renderer->Draw(views_);
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

void RaceScene::TickPause(const sim::InputFrame& input) {
    const std::optional<PauseRow> chosen = pause_.Tick(input);
    Hush();
    if (chosen == PauseRow::Restart) {
        next_ = circuit_ ? circuit_->AfterRestart() : RaceSceneName(parts_.setup);
    } else if (chosen == PauseRow::Quit && circuit_) {
        next_ = circuit_->AfterQuit();
    } else if (chosen == PauseRow::Quit) {
        next_ = trial_ ? std::string(kTrialEndScene) : MenuAfter(parts_.setup);
    }
}

void RaceScene::TickTrialQuestion(TimeTrialRace& trial, const sim::InputFrame& input) {
    const std::optional<TrialChoice> choice = trial.Ask(input);
    Hush();
    if (choice) {
        next_ = *choice == TrialChoice::RaceAgain ? RaceSceneName(parts_.setup)
                                                  : std::string(kTrialEndScene);
    }
}

void RaceScene::Hush() {
    hud_.TickPause(kSimTick);
    if (parts_.audio) {
        parts_.audio->StopLoops();
    }
}

bool RaceScene::IsTrialAsking() const {
    return trial_ && trial_->IsAsking(parts_.session.Clock());
}

void RaceScene::LeaveAfterResults() {
    const sim::RaceClock& clock = parts_.session.Clock();
    if (!circuit_) {
        if (clock.Phase() == sim::RacePhase::Results) {
            next_ = MenuAfter(parts_.setup);
        }
        return;
    }
    if (clock.Phase() == sim::RacePhase::Results && clock.PhaseTime() >= kCircuitResultsTime) {
        std::vector<std::optional<int>> places;
        places.reserve(parts_.session.Racers().size());
        for (std::size_t racer = 0; racer < parts_.session.Racers().size(); ++racer) {
            places.push_back(parts_.session.Progress().OfRacer(racer).finishPlace);
        }
        next_ = circuit_->AfterFinish(places);
    }
}

std::optional<PauseView> RaceScene::PauseShown() const {
    if (trial_ && trial_->IsAsking(parts_.session.Clock())) {
        return trial_->Question();
    }
    if (!pause_.IsOpen()) {
        return std::nullopt;
    }
    if (const std::optional<ConfirmRow> confirm = pause_.Confirming()) {
        return PauseView{.dialog = PauseDialog::Confirm,
                         .focus = static_cast<std::size_t>(*confirm)};
    }
    return PauseView{.dialog = PauseDialog::Menu,
                     .focus = static_cast<std::size_t>(pause_.Focus())};
}

}
