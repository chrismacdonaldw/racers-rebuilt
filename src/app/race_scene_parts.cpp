#include "app/race_scene_parts.hpp"

#include <chrono>
#include <utility>

#include "app/race_step.hpp"
#include "core/log.hpp"
#include "platform/clock.hpp"
#include "sim/powerup/every_effect.hpp"

namespace rr::app {

Result<sim::RaceTrack> LoadSceneTrack(const assets::Vfs& vfs, const RaceSceneSetup& setup) {
    return sim::LoadRaceTrack(vfs, setup.track, setup.isMirrored,
                              setup.isTimeTrial ? sim::BrickSet::TimeTrial : sim::BrickSet::Race);
}

sim::RaceSetup GridSetup(const RaceSceneSetup& scene, const RacerGrid& racers, std::uint64_t seed,
                         std::size_t slots) {
    sim::RaceSetup setup{.track = scene.track,
                         .firstSlot = scene.firstSlot,
                         .seed = seed,
                         .lapCount = scene.lapCount,
                         .respawn = scene.respawn,
                         .aiDriving = scene.aiDriving,
                         .effects = sim::EveryEffect()};
    setup.racers.resize(slots);
    for (std::size_t i = 0; i < slots; ++i) {
        setup.racers[i].chassis = racers.Chassis(i);
        setup.racers[i].localBounds = racers.LocalBounds(i);
        setup.racers[i].habits = racers.Habits(i);
    }
    for (std::size_t player = 0; player < scene.players; ++player) {
        setup.racers[kPlayerSlot + player].isPlayer = true;
    }
    return setup;
}

std::vector<SkidMarks> PlayerSkids(const assets::NameMap<render::Material>& materials,
                                   const RacerGrid& racers, std::size_t players) {
    std::vector<SkidMarks> skids;
    skids.reserve(players);
    for (std::size_t player = 0; player < players; ++player) {
        skids.emplace_back(materials, racers.Skids(kPlayerSlot + player));
    }
    return skids;
}

std::vector<PowerupQuads> PlayerPowerupQuads(const assets::NameMap<render::Material>& materials,
                                             std::size_t players) {
    std::vector<PowerupQuads> quads;
    quads.reserve(players);
    for (std::size_t player = 0; player < players; ++player) {
        quads.emplace_back(materials);
    }
    return quads;
}

RaceAudioSetup AudioSetup(const sim::RaceTrack& raceTrack, std::span<const std::int32_t> voiceBanks,
                          std::uint64_t seed) {
    return {.racerCount = voiceBanks.size(),
            .player = kPlayerSlot,
            .seed = seed,
            .ambience = raceTrack.events.table.sounds,
            .voiceBanks = voiceBanks};
}

std::unique_ptr<RaceAudio> LoadRaceAudio(const RaceOutputs& outputs, const assets::Vfs& vfs,
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

std::optional<RaceParticles> LoadRaceParticles(render::Renderer& renderer, const assets::Vfs& vfs,
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
