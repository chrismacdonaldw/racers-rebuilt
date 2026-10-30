#include "app/race_scene.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <format>
#include <optional>
#include <system_error>
#include <utility>

#include "app/checkpoint_gates.hpp"
#include "app/collision_wireframe.hpp"
#include "app/frame_loop.hpp"
#include "app/race_status.hpp"
#include "app/track_model.hpp"
#include "app/track_space.hpp"
#include "core/log.hpp"
#include "platform/clock.hpp"
#include "sim/replay/snapshot_hash.hpp"

namespace rr::app {
namespace {

constexpr std::string_view kRacePrefix = "race:";
constexpr std::string_view kRespawnField = ":respawn=";
constexpr std::string_view kRespawnOff = "off";
constexpr std::size_t kPlayer = 0;
constexpr std::uint64_t kHashPrime = 1099511628211ULL;

sim::RaceSetup FullGrid(const RaceSceneSetup& scene, std::uint64_t seed, std::size_t slots) {
    sim::RaceSetup setup{.track = scene.track, .seed = seed, .respawn = scene.respawn};
    setup.racers.resize(slots);
    setup.racers[kPlayer].isPlayer = true;
    return setup;
}

sim::EntityId RacerId(std::size_t racer) {
    return sim::EntityId{static_cast<std::uint32_t>(racer + 1)};
}

sim::SnapshotFlags FlagsFor(const sim::RaceSession& session, std::size_t racer) {
    return session.WasRespawned(racer) ? sim::SnapshotFlags::Teleported : sim::SnapshotFlags::None;
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
    Result<std::vector<render::MeshDraw>> car =
        model.and_then([&](const TrackModel&) { return UploadStandInCar(renderer, loader); });
    Result<sim::RaceTrack> loaded = car.and_then(
        [&](const std::vector<render::MeshDraw>&) { return sim::LoadRaceTrack(vfs, track); });
    if (!loaded) {
        return std::unexpected(std::move(loaded.error()));
    }
    auto raceTrack = std::make_unique<sim::RaceTrack>(std::move(*loaded));
    auto world = std::make_unique<sim::WorldList>(raceTrack->collision.Track());
    Result<sim::RaceSession> session = sim::RaceSession::Create(
        FullGrid(setup, seed, raceTrack->grid.size()), {.checkpoints = &raceTrack->checkpoints,
                                                        .grid = raceTrack->grid,
                                                        .world = world.get(),
                                                        .lowestTrackZ = raceTrack->lowestTrackZ});
    if (!session) {
        return std::unexpected(std::move(session.error()));
    }
    return std::unique_ptr<RaceScene>(new RaceScene({.renderer = &renderer,
                                                     .track = std::string(track),
                                                     .trackDraws = std::move(model->draws),
                                                     .carDraws = std::move(*car),
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
    for (std::size_t i = 0; i < parts_.session.Racers().size(); ++i) {
        draws_.insert(draws_.end(), parts_.carDraws.begin(), parts_.carDraws.end());
    }
}

void RaceScene::ApplySettings(const settings::Registry& settings) {
    showsCheckpoints_ = settings.Get(kShowCheckpoints);
    showsCollision_ = settings.Get(kCollisionWireframe);
}

void RaceScene::Tick(const sim::InputFrame& input) {
    controls_[kPlayer] = sim::ControlsFrom(input);
    parts_.session.Tick(kSimTick, controls_);
    if (parts_.session.WasRespawned(kPlayer)) {
        chase_.JumpTo(PlayerCar());
    } else {
        chase_.Follow(PlayerCar());
    }
}

void RaceScene::Snapshot(sim::RenderSnapshot& out) const {
    const sim::RaceSession& session = parts_.session;
    out.entities.clear();
    std::uint64_t hash = 0;
    for (std::size_t i = 0; i < session.Racers().size(); ++i) {
        const sim::Racer& racer = session.Racers()[i];
        out.entities.push_back({.id = RacerId(i),
                                .transform = CarToEngine(racer.Pose()),
                                .flags = FlagsFor(session, i)});
        hash = (hash * kHashPrime) ^ sim::HashRacerState(racer);
    }
    out.camera = {.transform = chase_.Pose(PlayerCar()), .flags = FlagsFor(session, kPlayer)};
    out.simStateHash = hash;
}

void RaceScene::Render(const sim::RenderSnapshot& interpolated) {
    const std::size_t carDraws = parts_.carDraws.size();
    for (std::size_t racer = 0; racer < interpolated.entities.size(); ++racer) {
        const std::size_t first = parts_.trackDraws.size() + (racer * carDraws);
        for (std::size_t i = first; i < first + carDraws; ++i) {
            draws_[i].transform = interpolated.entities[racer].transform;
        }
    }
    render::Camera camera = parts_.camera;
    camera.transform = interpolated.camera.transform;
    DrawDebugLines();
    FormatRaceStatus(parts_.session, kPlayer, status_);
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
    return CarToEngine(parts_.session.Racers()[kPlayer].Pose());
}

void RaceScene::DrawDebugLines() {
    debugLines_.clear();
    if (showsCheckpoints_) {
        DrawGates(parts_.raceTrack->checkpoints, parts_.session.Progress().OfRacer(kPlayer).laps,
                  debugLines_);
    }
    if (showsCollision_) {
        debugLines_.insert(debugLines_.end(), collisionLines_.begin(), collisionLines_.end());
    }
}

}
