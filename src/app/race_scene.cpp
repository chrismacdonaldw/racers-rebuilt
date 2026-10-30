#include "app/race_scene.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <system_error>
#include <utility>

#include "app/checkpoint_gates.hpp"
#include "app/collision_wireframe.hpp"
#include "app/race_status.hpp"
#include "app/race_step.hpp"
#include "app/track_model.hpp"
#include "app/track_space.hpp"
#include "core/log.hpp"
#include "platform/clock.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kRacePrefix = "race:";
constexpr std::string_view kRespawnField = ":respawn=";
constexpr std::string_view kRespawnOff = "off";

sim::RaceSetup FullGrid(const RaceSceneSetup& scene, const RacerGrid& racers, std::uint64_t seed,
                        std::size_t slots) {
    sim::RaceSetup setup{.track = scene.track, .seed = seed, .respawn = scene.respawn};
    setup.racers.resize(slots);
    for (std::size_t i = 0; i < slots; ++i) {
        setup.racers[i].chassis = racers.Chassis(i);
    }
    setup.racers[kPlayerSlot].isPlayer = true;
    return setup;
}

float MsSince(std::optional<Nanoseconds>& last) {
    const Nanoseconds now = platform::Now();
    const Nanoseconds elapsed = last ? now - *last : Nanoseconds{};
    last = now;
    return std::chrono::duration<float, std::milli>(elapsed).count();
}

std::optional<sim::RespawnRule> ParseRespawn(std::string_view text) {
    if (text == kRespawnOff) {
        return sim::RespawnRule{.isEnabled = false};
    }
    Milliseconds::rep timeout = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), timeout);
    if (error != std::errc() || end != text.data() + text.size() || timeout <= 0) {
        return std::nullopt;
    }
    return sim::RespawnRule{.timeout = Milliseconds{timeout}};
}

}

std::string RaceSceneName(const RaceSceneSetup& setup) {
    if (!setup.respawn.isEnabled) {
        return std::format("{}{}{}{}", kRacePrefix, setup.track, kRespawnField, kRespawnOff);
    }
    return std::format("{}{}{}{}", kRacePrefix, setup.track, kRespawnField,
                       setup.respawn.timeout.count());
}

bool IsRaceScene(std::string_view scene) {
    return scene.starts_with(kRacePrefix);
}

Result<RaceSceneSetup> ParseRaceScene(std::string_view scene) {
    const std::size_t field = scene.find(kRespawnField);
    const bool hasTrack = field != std::string_view::npos && field > kRacePrefix.size();
    const std::optional<sim::RespawnRule> respawn =
        IsRaceScene(scene) && hasTrack ? ParseRespawn(scene.substr(field + kRespawnField.size()))
                                       : std::nullopt;
    if (!respawn) {
        return Fail(ErrorCode::InvalidData, std::format("'{}' isn't a race scene", scene));
    }
    return RaceSceneSetup{
        .track = std::string(scene.substr(kRacePrefix.size(), field - kRacePrefix.size())),
        .respawn = *respawn};
}

Result<std::unique_ptr<RaceScene>> RaceScene::Create(render::Renderer& renderer,
                                                     const assets::Vfs& vfs,
                                                     assets::TrackLoader& loader,
                                                     const RaceSceneSetup& setup,
                                                     std::uint64_t seed, Nanoseconds started) {
    const std::string& track = setup.track;
    Result<TrackModel> model = loader.Load(std::format("GAMEDATA/{}", track))
                                   .and_then([&](const assets::TrackAssets& assets) {
                                       return UploadTrack(renderer, assets);
                                   });
    Result<sim::RaceTrack> loaded =
        model.and_then([&](const TrackModel&) { return sim::LoadRaceTrack(vfs, track); });
    Result<RacerGrid> racers = loaded.and_then([&](const sim::RaceTrack& race) {
        return RacerGrid::Load(renderer, vfs, loader, track, race.grid.size());
    });
    if (!racers) {
        return std::unexpected(std::move(racers.error()));
    }
    auto raceTrack = std::make_unique<sim::RaceTrack>(std::move(*loaded));
    auto world = std::make_unique<sim::WorldList>(raceTrack->collision.Track());
    Result<sim::RaceSession> session =
        sim::RaceSession::Create(FullGrid(setup, *racers, seed, raceTrack->grid.size()),
                                 {.checkpoints = &raceTrack->checkpoints,
                                  .grid = raceTrack->grid,
                                  .world = world.get(),
                                  .lowestTrackZ = raceTrack->lowestTrackZ,
                                  .routes = &raceTrack->routes});
    if (!session) {
        return std::unexpected(std::move(session.error()));
    }
    return std::unique_ptr<RaceScene>(new RaceScene({.renderer = &renderer,
                                                     .track = std::string(track),
                                                     .trackDraws = std::move(model->draws),
                                                     .racers = std::move(*racers),
                                                     .camera = TrackCamera(model->bounds),
                                                     .raceTrack = std::move(raceTrack),
                                                     .world = std::move(world),
                                                     .session = std::move(*session),
                                                     .started = started}));
}

RaceScene::RaceScene(Parts parts)
    : parts_(std::move(parts)),
      chase_(PlayerCar()),
      controls_(parts_.session.Racers().size()),
      collisionLines_(BuildCollisionWireframe(parts_.raceTrack->collision)) {
    draws_ = parts_.trackDraws;
    draws_.insert(draws_.end(), parts_.racers.Draws().begin(), parts_.racers.Draws().end());
}

void RaceScene::ApplySettings(const settings::Registry& settings) {
    showsCheckpoints_ = settings.Get(kShowCheckpoints);
    showsCollision_ = settings.Get(kCollisionWireframe);
}

void RaceScene::Tick(const sim::InputFrame& input) {
    StepRace(parts_.session, input, controls_);
    if (parts_.session.WasRespawned(kPlayerSlot)) {
        chase_.JumpTo(PlayerCar());
    } else {
        chase_.Follow(PlayerCar());
    }
}

void RaceScene::Snapshot(sim::RenderSnapshot& out) const {
    SnapshotRace(parts_.session, out);
    out.camera = {.transform = chase_.Pose(PlayerCar()), .flags = out.entities[kPlayerSlot].flags};
}

void RaceScene::Render(const sim::RenderSnapshot& interpolated) {
    parts_.racers.Update(interpolated.entities, interpolated.racers, MsSince(lastRender_));
    std::ranges::copy(parts_.racers.Draws(),
                      draws_.begin() + static_cast<std::ptrdiff_t>(parts_.trackDraws.size()));
    render::Camera camera = parts_.camera;
    camera.transform = interpolated.camera.transform;
    DrawDebugLines();
    FormatRaceStatus(parts_.session, kPlayerSlot, status_);
    Result<void> drawn =
        parts_.renderer->Draw({.camera = camera, .meshes = draws_, .lines = debugLines_});
    if (!drawn) {
        log::Error("{}", drawn.error().message);
        return;
    }
    if (!hasDrawn_) {
        hasDrawn_ = true;
        log::Info(
            "{}: race of {} ready, first frame {} ms after loading began", parts_.track,
            parts_.session.Racers().size(),
            std::chrono::duration_cast<Milliseconds>(platform::Now() - parts_.started).count());
    }
}

Transform RaceScene::PlayerCar() const {
    return CarToEngine(parts_.session.Racers()[kPlayerSlot].Pose());
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
}

}
