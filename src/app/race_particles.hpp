#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "assets/surface_table.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "render/particle_bank.hpp"
#include "render/renderer.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/race/race_session.hpp"

namespace rr::app {

inline constexpr std::size_t kCommonParticleSlots = 10;
inline constexpr std::size_t kTrackParticleSlots = 4;

/// Power-ups use the common `EMITTER` file and hazards the track's own.
enum class ParticleFile : std::uint8_t { Common, Track };

/// Reads the race and never changes it. `surfaces` must outlive it.
class RaceParticles {
public:
    RaceParticles(render::ParticleBankSetup common, render::ParticleBankSetup track,
                  std::span<const assets::SurfaceEntry> surfaces, std::size_t player);

    /// `track` is a race track folder under `GAMEDATA`, such as `RACEC0R0`.
    [[nodiscard]] static Result<RaceParticles> Load(
        render::Renderer& renderer, const assets::Vfs& vfs, assets::TrackLoader& loader,
        std::string_view track, std::span<const assets::SurfaceEntry> surfaces, std::size_t player);

    void Update(const sim::RaceSession& session, render::FloatMilliseconds elapsed);
    void AppendQuads(const Transform& camera, std::vector<render::QuadDraw>& out) const;

    [[nodiscard]] render::ParticleBank& Bank(ParticleFile file);

private:
    struct WheelEmitter {
        std::optional<std::string_view> name;
        ParticleFile file = ParticleFile::Common;
        std::optional<render::EmitterHandle> handle;
    };

    [[nodiscard]] std::optional<std::string_view> ParticleOf(const sim::WheelProbe& wheel) const;
    void UpdateWheel(WheelEmitter& wheel, std::optional<std::string_view> name,
                     const render::EmitterPlacement& placement);
    void Start(WheelEmitter& wheel, const render::EmitterPlacement& placement);

    std::array<render::ParticleBank, 2> banks_;
    std::span<const assets::SurfaceEntry> surfaces_;
    std::size_t player_;
    std::array<WheelEmitter, sim::kWheelCount> wheels_{};
};

}
