#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "app/powerup/powerup_quads.hpp"
#include "app/race_audio.hpp"
#include "app/race_particles.hpp"
#include "app/race_scene_name.hpp"
#include "app/racer/racer_grid.hpp"
#include "app/racer/skid_marks.hpp"
#include "app/sound_banks.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "audio/mixer.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/race/race_setup.hpp"
#include "sim/race/race_track.hpp"

namespace rr::app {

/// Where a race draws and plays. Each must outlive the scene.
struct RaceOutputs {
    render::Renderer* renderer = nullptr;
    audio::Mixer* mixer = nullptr;
    SoundBanks* banks = nullptr;
};

template <typename T>
std::vector<T> PerSlot(const RacerGrid& racers, std::size_t slots,
                       T (RacerGrid::*value)(std::size_t) const) {
    std::vector<T> values(slots);
    for (std::size_t i = 0; i < slots; ++i) {
        values[i] = (racers.*value)(i);
    }
    return values;
}

struct RacerSounds {
    std::vector<std::int32_t> voices;
    std::vector<float> enginePitches;
};

[[nodiscard]] RacerSounds RacerSoundsOf(const RacerGrid& racers, std::size_t slots);

[[nodiscard]] Result<sim::RaceTrack> LoadSceneTrack(const assets::Vfs& vfs,
                                                    const RaceSceneSetup& setup);

[[nodiscard]] sim::RaceSetup GridSetup(const assets::Vfs& vfs, const RaceSceneSetup& scene,
                                       const RacerGrid& racers, std::uint64_t seed,
                                       std::size_t slots);

[[nodiscard]] std::vector<SkidMarks> PlayerSkids(const assets::NameMap<render::Material>& materials,
                                                 const RacerGrid& racers, std::size_t players);

[[nodiscard]] std::vector<PowerupQuads> PlayerPowerupQuads(
    const assets::NameMap<render::Material>& materials, std::size_t players);

[[nodiscard]] RaceAudioSetup AudioSetup(const assets::Vfs& vfs, const RaceSceneSetup& setup,
                                        const sim::RaceTrack& raceTrack, const RacerSounds& racers,
                                        std::uint64_t seed);

/// Null when the race's sounds don't load.
[[nodiscard]] std::unique_ptr<RaceAudio> LoadRaceAudio(const RaceOutputs& outputs,
                                                       const assets::Vfs& vfs,
                                                       std::string_view track,
                                                       const RaceAudioSetup& setup);

[[nodiscard]] std::optional<RaceParticles> LoadRaceParticles(
    render::Renderer& renderer, const assets::Vfs& vfs, assets::TrackLoader& loader,
    std::string_view track, std::span<const assets::SurfaceEntry> surfaces);

[[nodiscard]] float MsSince(std::optional<Nanoseconds>& last);

}
