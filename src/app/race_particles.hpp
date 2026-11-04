#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "app/hazard/hazard_particles.hpp"
#include "app/powerup/blast_fx.hpp"
#include "app/powerup/powerup_particles.hpp"
#include "app/racer/car_particles.hpp"
#include "app/racer/skid_marks.hpp"
#include "app/racer/wheel_spray.hpp"
#include "assets/surface_table.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "render/fx_system.hpp"
#include "render/particle_bank.hpp"
#include "render/renderer.hpp"
#include "sim/physics/body_state.hpp"
#include "sim/race/race_session.hpp"

namespace rr::app {

inline constexpr std::size_t kCommonParticleSlots = 10;
inline constexpr std::size_t kTrackParticleSlots = 4;

/// Power-ups use the common `EMITTER` file and hazards the track's own.
enum class ParticleFile : std::uint8_t { Common, Track };

using FxFactory = std::function<std::unique_ptr<render::FxSystem>()>;

/// Reads the race and never changes it. `surfaces` must outlive it.
class RaceParticles {
public:
    RaceParticles(render::ParticleBankSetup common, render::ParticleBankSetup track,
                  FxFactory makeFx, std::span<const assets::SurfaceEntry> surfaces,
                  std::size_t player);

    /// `track` is a race track folder under `GAMEDATA`, such as `RACEC0R0`.
    [[nodiscard]] static Result<RaceParticles> Load(
        render::Renderer& renderer, const assets::Vfs& vfs, assets::TrackLoader& loader,
        std::string_view track, std::span<const assets::SurfaceEntry> surfaces, std::size_t player);

    /// `camera` is the player's, in engine space.
    void Tick(const sim::RaceSession& session, const Transform& camera, Milliseconds elapsed);
    /// Each player's car is at slot `player` + its index in `skidding`.
    void Update(const sim::RaceSession& session,
                std::span<const std::optional<SkidWheels>> skidding,
                render::FloatMilliseconds elapsed);
    void AppendQuads(const Transform& camera, std::vector<render::QuadDraw>& out) const;
    /// Draws the race's better particle effects in place of the emitters they replace.
    void SetBetter(bool isBetter);

    [[nodiscard]] render::ParticleBank& Bank(ParticleFile file);

private:
    struct WheelEmitter {
        std::optional<std::string_view> name;
        ParticleFile file = ParticleFile::Common;
        std::optional<render::EmitterHandle> handle;
    };

    struct PlayerEmitters {
        std::array<WheelEmitter, sim::kWheelCount> wheels{};
        CarParticles::Puffs puffs;
    };

    void UpdatePlayer(const sim::Racer& racer, const std::optional<SkidWheels>& skidding,
                      PlayerEmitters& emitters);

    void UpdateWheel(WheelEmitter& wheel, std::optional<std::string_view> name,
                     const render::EmitterPlacement& placement);
    void Start(WheelEmitter& wheel, const render::EmitterPlacement& placement);

    std::array<render::ParticleBank, 2> banks_;
    FxFactory makeFx_;
    std::unique_ptr<render::FxSystem> fx_;
    BlastFx blasts_;
    WheelSpray spray_;
    bool isBetter_ = false;
    std::span<const assets::SurfaceEntry> surfaces_;
    std::size_t player_;
    std::vector<PlayerEmitters> players_;
    std::vector<CarParticles::Puffs> computers_;
    CarParticles cars_;
    PowerupParticles powerups_;
    HazardParticles hazards_;
};

}
