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
#include "app/track_model.hpp"
#include "app/track_space.hpp"
#include "assets/circuit_database.hpp"
#include "core/log.hpp"
#include "platform/clock.hpp"
#include "sim/powerup/green_effects.hpp"
#include "sim/powerup/red_effects.hpp"
#include "sim/powerup/shield_effect.hpp"
#include "sim/powerup/yellow_effects.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kRacePrefix = "race:";
constexpr std::string_view kRespawnKey = "respawn";
constexpr std::string_view kRacerKey = "racer";
constexpr std::string_view kRespawnOff = "off";
constexpr std::uint64_t kHashPrime = 1099511628211ULL;

sim::RaceSetup FullGrid(const RaceSceneSetup& scene, const RacerGrid& racers, std::uint64_t seed,
                        std::size_t slots) {
    sim::RaceSetup setup{.track = scene.track, .seed = seed, .respawn = scene.respawn};
    sim::AddGreenEffects(setup.effects);
    sim::AddBlueEffects(setup.effects);
    sim::AddRedEffects(setup.effects);
    sim::AddYellowEffects(setup.effects);
    setup.racers.resize(slots);
    for (std::size_t i = 0; i < slots; ++i) {
        setup.racers[i].chassis = racers.Chassis(i);
        setup.racers[i].localBounds = racers.LocalBounds(i);
        setup.racers[i].habits = racers.Habits(i);
    }
    setup.racers[kPlayerSlot].isPlayer = true;
    return setup;
}

std::vector<std::int32_t> NameIndexes(const RacerGrid& racers, std::size_t slots) {
    std::vector<std::int32_t> indexes(slots);
    for (std::size_t i = 0; i < slots; ++i) {
        indexes[i] = racers.NameIndex(i);
    }
    return indexes;
}

float MsSince(std::optional<Nanoseconds>& last) {
    const Nanoseconds now = platform::Now();
    const Nanoseconds elapsed = last ? now - *last : Nanoseconds{};
    last = now;
    return std::chrono::duration<float, std::milli>(elapsed).count();
}

template <typename T>
std::optional<T> ParseNumber(std::string_view text) {
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<sim::RespawnRule> ParseRespawn(std::string_view text) {
    if (text == kRespawnOff) {
        return sim::RespawnRule{.isEnabled = false};
    }
    const std::optional<Milliseconds::rep> timeout = ParseNumber<Milliseconds::rep>(text);
    if (!timeout || *timeout <= 0) {
        return std::nullopt;
    }
    return sim::RespawnRule{.timeout = Milliseconds{*timeout}};
}

bool ReadField(std::string_view field, RaceSceneSetup& setup,
               std::optional<sim::RespawnRule>& respawn) {
    const std::size_t equals = field.find('=');
    const std::string_view key = field.substr(0, equals);
    const std::string_view value =
        equals == std::string_view::npos ? std::string_view{} : field.substr(equals + 1);
    if (key == kRespawnKey) {
        respawn = ParseRespawn(value);
        return respawn.has_value();
    }
    if (key == kRacerKey) {
        setup.racer = ParseNumber<std::size_t>(value).value_or(assets::kGridSlots);
        return setup.racer < assets::kGridSlots;
    }
    return false;
}

sim::RaceCourse CourseOf(const sim::RaceTrack& track, const sim::WorldList& world) {
    return {.checkpoints = &track.checkpoints,
            .grid = track.grid,
            .world = &world,
            .lowestTrackZ = track.lowestTrackZ,
            .routes = &track.routes,
            .bricks = &track.bricks,
            .targets = track.targets,
            .events = &track.events,
            .collision = &track.collision,
            .entities = &track.entities};
}

}

std::string RaceSceneName(const RaceSceneSetup& setup) {
    const std::string respawn = setup.respawn.isEnabled
                                    ? std::to_string(setup.respawn.timeout.count())
                                    : std::string(kRespawnOff);
    return std::format("{}{}:{}={}:{}={}", kRacePrefix, setup.track, kRespawnKey, respawn,
                       kRacerKey, setup.racer);
}

bool IsRaceScene(std::string_view scene) {
    return scene.starts_with(kRacePrefix);
}

Result<RaceSceneSetup> ParseRaceScene(std::string_view scene) {
    RaceSceneSetup setup;
    std::optional<sim::RespawnRule> respawn;
    bool isValid = IsRaceScene(scene);
    std::string_view rest = isValid ? scene.substr(kRacePrefix.size()) : std::string_view{};
    const std::size_t trackEnd = rest.find(':');
    setup.track = rest.substr(0, trackEnd);
    rest = trackEnd == std::string_view::npos ? std::string_view{} : rest.substr(trackEnd + 1);
    for (const auto field : std::views::split(rest, ':')) {
        isValid = isValid && ReadField(std::string_view(field), setup, respawn);
    }
    if (!isValid || setup.track.empty() || !respawn) {
        return Fail(ErrorCode::InvalidData, std::format("'{}' isn't a race scene", scene));
    }
    setup.respawn = *respawn;
    return setup;
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
        return RacerGrid::Load(renderer, vfs, loader, track, race.grid.size(), setup.racer);
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
        FullGrid(setup, *racers, seed, raceTrack->grid.size()), CourseOf(*raceTrack, *world));
    if (!session) {
        return std::unexpected(std::move(session.error()));
    }
    Result<std::unique_ptr<RaceAudio>> audio = RaceAudio::Load(
        *outputs.mixer, *outputs.banks, vfs, track,
        {.racerCount = session->Racers().size(), .player = kPlayerSlot, .seed = seed});
    if (!audio) {
        log::Warn("{}, racing in silence", audio.error().message);
    }
    Result<RaceParticles> loadedParticles = RaceParticles::Load(
        renderer, vfs, loader, track, raceTrack->collision.surfaces, kPlayerSlot);
    std::optional<RaceParticles> particles;
    if (loadedParticles) {
        particles.emplace(std::move(*loadedParticles));
    } else {
        log::Warn("{}, racing without particles", loadedParticles.error().message);
    }
    return std::unique_ptr<RaceScene>(
        new RaceScene({.renderer = &renderer,
                       .vfs = &vfs,
                       .setup = setup,
                       .trackDraws = std::move(model->draws),
                       .racers = std::move(*racers),
                       .bricks = std::move(*bricks),
                       .powerups = LoadPowerupModels(renderer, vfs, loader),
                       .camera = TrackCamera(model->bounds),
                       .raceTrack = std::move(raceTrack),
                       .entityDraws = std::move(entityDraws),
                       .world = std::move(world),
                       .session = std::move(*session),
                       .audio = std::move(audio).value_or(nullptr),
                       .particles = std::move(particles),
                       .started = started}));
}

RaceScene::RaceScene(Parts parts)
    : parts_(std::move(parts)),
      powerupVisuals_(parts_.powerups.models),
      powerupQuads_(parts_.powerups.quadMaterials),
      chase_(PlayerCar()),
      hud_(*parts_.renderer, *parts_.vfs, kPlayerSlot,
           NameIndexes(parts_.racers, parts_.session.Racers().size())),
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
    if (parts_.session.WasRespawned(kPlayerSlot)) {
        chase_.JumpTo(PlayerCar());
    } else {
        chase_.Follow(PlayerCar());
    }
    if (parts_.audio) {
        parts_.audio->Hear(parts_.session, chase_.Pose(PlayerCar()), kSimTick);
    }
    if (parts_.particles) {
        parts_.particles->Tick(parts_.session, kSimTick);
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

void RaceScene::Render(const sim::RenderSnapshot& interpolated) {
    const float elapsedMs = MsSince(lastRender_);
    parts_.racers.Update(interpolated.entities, interpolated.racers, elapsedMs);
    draws_.resize(parts_.trackDraws.size() + parts_.racers.Draws().size());
    std::ranges::copy(parts_.racers.Draws(),
                      draws_.begin() + static_cast<std::ptrdiff_t>(parts_.trackDraws.size()));
    parts_.entityDraws.Update(interpolated.entityClocks);
    draws_.insert(draws_.end(), parts_.entityDraws.Draws().begin(),
                  parts_.entityDraws.Draws().end());
    parts_.bricks.AppendDraws(interpolated.bricks, draws_);
    powerupVisuals_.Update(interpolated, elapsedMs);
    draws_.insert(draws_.end(), powerupVisuals_.Draws().begin(), powerupVisuals_.Draws().end());
    render::Camera camera = parts_.camera;
    camera.transform = interpolated.camera.transform;
    DrawDebugLines();
    quads_.clear();
    if (parts_.particles) {
        parts_.particles->Update(parts_.session,
                                 render::FloatMilliseconds{pause_.IsOpen() ? 0.0F : elapsedMs});
        parts_.particles->AppendQuads(camera.transform, quads_);
    }
    powerupQuads_.Update(interpolated, camera.transform, elapsedMs);
    quads_.insert(quads_.end(), powerupQuads_.Quads().begin(), powerupQuads_.Quads().end());
    Result<void> drawn = parts_.renderer->Draw(
        {.camera = camera,
         .meshes = draws_,
         .quads = quads_,
         .lines = debugLines_,
         .sprites = hud_.Draw(parts_.session, interpolated.racers[kPlayerSlot], PauseFocus())});
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
