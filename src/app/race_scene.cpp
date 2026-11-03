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
#include "app/hud/race_map_view.hpp"
#include "app/menu/menu_scenes.hpp"
#include "app/powerup_tools.hpp"
#include "app/race_scene_parts.hpp"
#include "app/race_step.hpp"
#include "app/racer/far_cars.hpp"
#include "app/racer/racer_fx.hpp"
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
        racers.and_then([&](const RacerGrid&) { return BrickModels::Load(renderer, loader); });
    Result<SkyBackdrop> sky = bricks.and_then([&](const BrickModels&) {
        return SkyBackdrop::Upload(renderer, std::move(model->skyModels));
    });
    if (!sky) {
        return std::unexpected(std::move(sky.error()));
    }
    auto raceTrack = std::make_unique<sim::RaceTrack>(std::move(*loaded));
    auto world = std::make_unique<sim::WorldList>(raceTrack->collision.Track());
    TrackEntityDraws entityDraws(model->animatedModels, raceTrack->entities);
    Result<sim::RaceSession> session = sim::RaceSession::Create(
        GridSetup(setup, *racers, seed, racers->Count()), sim::CourseOf(*raceTrack, *world));
    if (!session) {
        return std::unexpected(std::move(session.error()));
    }
    HiddenModels hiddenModels(raceTrack->events.table.modelDistances,
                              session->TrackHazards().Models().Names(), model->entityDraws,
                              raceTrack->entities);
    GhostTrail ghostTrail(model->draws, model->entityDraws, raceTrack->entities);
    WaterScroll waterScroll(raceTrack->hazards, model->entityDraws);
    const std::vector<std::int32_t> voices =
        PerSlot(*racers, session->Racers().size(), &RacerGrid::Voice);
    std::unique_ptr<RaceAudio> audio =
        LoadRaceAudio(outputs, vfs, setup.track, AudioSetup(setup, *raceTrack, voices, seed));
    std::optional<RaceParticles> particles =
        LoadRaceParticles(renderer, vfs, loader, setup.track, raceTrack->collision.surfaces);
    return std::unique_ptr<RaceScene>(
        new RaceScene({.renderer = &renderer,
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
                       .entityDraws = std::move(entityDraws),
                       .farModels = FarModels(std::move(model->farModels)),
                       .hiddenModels = std::move(hiddenModels),
                       .ghostTrail = std::move(ghostTrail),
                       .waterScroll = std::move(waterScroll),
                       .world = std::move(world),
                       .session = std::move(*session),
                       .audio = std::move(audio),
                       .particles = std::move(particles),
                       .map = LoadTrackMap(vfs, setup.track, setup.isMirrored, model->textures),
                       .started = started}));
}

RaceScene::RaceScene(Parts parts)
    : parts_(std::move(parts)),
      groundDecals_(parts_.decalSurface),
      shadows_(parts_.racerFx, PerSlot(parts_.racers, parts_.racers.Count(), &RacerGrid::Chassis)),
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
    hud_.SetMap(parts_.map);
    if (parts_.setup.isDemo) {
        hud_.ShowDemo();
    }
}

void RaceScene::ApplySettings(const settings::Registry& settings) {
    showsCheckpoints_ = settings.Get(kShowCheckpoints);
    showsCollision_ = settings.Get(kCollisionWireframe);
    showsTriggers_ = settings.Get(kShowTriggerSpheres);
    parts_.farModels.SetHidden(!settings.Get(kFarCarsSetting), !settings.Get(kFarScenerySetting));
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
    return isRacing && parts_.setup.cheats.Has(sim::Cheat::FastForward) ? kFastForwardScale : 1.0;
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
    draws_.insert(draws_.end(), parts_.entityDraws.Draws().begin(),
                  parts_.entityDraws.Draws().end());
    parts_.hiddenModels.Apply(interpolated.hiddenModels, parts_.trackDraws, parts_.entityDraws,
                              parts_.trackDraws.size() + parts_.racers.Draws().size(), draws_);
    parts_.ghostTrail.Append(parts_.session.TrackHazards(), interpolated.entityClocks, draws_);
    parts_.waterScroll.Apply(draws_);
    parts_.bricks.AppendDraws(interpolated.bricks, draws_);
    powerupVisuals_.Update(interpolated, simMs);
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
                    std::span(cameras).first(parts_.farModels.HidesCars() ? cameras_.Count() : 0),
                    groundDecals_, carMarks_);
    for (std::size_t player = 0; player < skids_.size(); ++player) {
        const std::size_t slot = kPlayerSlot + player;
        skids_[player].Update(interpolated.entities[slot], interpolated.racers[slot], simMs);
        skids_[player].Append(groundDecals_, carMarks_);
        skidding_.at(player) = skids_[player].Skidding();
    }
}

render::RenderView RaceScene::PlayerView(const sim::RenderSnapshot& interpolated,
                                         std::size_t player, float simMs) {
    const std::size_t slot = kPlayerSlot + player;
    render::Camera lens = PlayerLens(parts_.camera, cameras_.Count());
    lens.verticalFovDegrees = cameras_.FovDegrees(player);
    const render::Viewport viewport = PlayerViewport(player, cameras_.Count());
    const sim::EffectSnapshot* flight =
        FindWarp(interpolated.effects, slot, sim::WarpPhase::Active);
    if (flight != nullptr && parts_.warpTube) {
        parts_.warpTube->Fly(flight->phaseLeftMs, interpolated.entities[slot].transform,
                             parts_.racers.SlotDraws(slot));
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
    if (parts_.particles) {
        parts_.particles->AppendQuads(camera.transform, quads);
    }
    PowerupQuads& powerupQuads = powerupQuads_[player];
    powerupQuads.Update(interpolated, camera.transform, simMs);
    quads.insert(quads.end(), powerupQuads.Quads().begin(), powerupQuads.Quads().end());
    std::vector<render::MeshDraw>& sky = skyDraws_.at(player);
    parts_.sky.Place(parts_.session.Resources().Sky(), camera.transform.position, skyShownMs_, sky);
    return {.camera = camera,
            .backdrop = sky,
            .meshes = parts_.farModels.Cull(player, camera.transform.position, draws_),
            .quads = quads,
            .lines = debugLines_,
            .viewport = viewport};
}

void RaceScene::Render(const sim::RenderSnapshot& interpolated) {
    const float elapsedMs = MsSince(lastRender_) * static_cast<float>(TimeScale());
    const float simMs = pause_.IsOpen() || IsTrialAsking() ? 0.0F : elapsedMs;
    skyShownMs_ += simMs;
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
    std::array<std::optional<render::SpriteDraw>, sim::kMaxPlayers> flashes{};
    for (std::size_t player = 0; player < cameras_.Count(); ++player) {
        const sim::EffectSnapshot* start =
            FindWarp(interpolated.effects, kPlayerSlot + player, sim::WarpPhase::Starting);
        if (start != nullptr) {
            flashes.at(player) = WarpFlash(start->phaseLeftMs);
        }
    }
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

void RaceScene::ApplyChosenTools() {
    for (const std::function<void(sim::RaceSession&)>& tool : chosenTools_) {
        tool(parts_.session);
    }
    chosenTools_.clear();
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
