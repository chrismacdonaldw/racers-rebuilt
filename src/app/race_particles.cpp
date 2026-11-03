#include "app/race_particles.hpp"

#include <utility>

#include "app/particle_assets.hpp"
#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr float kMinSpeed = 0.008F;
constexpr float kVelocityScale = 0.6F * 1000.0F;
constexpr std::size_t kRearRight = 2;
constexpr std::size_t kRearLeft = 3;

}

RaceParticles::RaceParticles(render::ParticleBankSetup common, render::ParticleBankSetup track,
                             std::span<const assets::SurfaceEntry> surfaces, std::size_t player)
    : banks_{render::ParticleBank(std::move(common)), render::ParticleBank(std::move(track))},
      surfaces_(surfaces),
      player_(player),
      hazards_(player) {}

Result<RaceParticles> RaceParticles::Load(render::Renderer& renderer, const assets::Vfs& vfs,
                                          assets::TrackLoader& loader, std::string_view track,
                                          std::span<const assets::SurfaceEntry> surfaces,
                                          std::size_t player) {
    Result<render::ParticleBankSetup> common =
        LoadParticleBank(renderer, vfs, loader, CommonParticleFiles(), kCommonParticleSlots);
    Result<render::ParticleBankSetup> trackBank =
        common.and_then([&](const render::ParticleBankSetup&) {
            return TrackParticleFiles(vfs, track).and_then([&](const ParticleFiles& files) {
                return LoadParticleBank(renderer, vfs, loader, files, kTrackParticleSlots);
            });
        });
    if (!trackBank) {
        return std::unexpected(std::move(trackBank.error()));
    }
    return RaceParticles(std::move(*common), std::move(*trackBank), surfaces, player);
}

void RaceParticles::Tick(const sim::RaceSession& session, const Transform& camera,
                         Milliseconds elapsed) {
    cars_.Tick(session.Racers(), Bank(ParticleFile::Common));
    powerups_.Tick(session, Bank(ParticleFile::Common), elapsed);
    hazards_.Tick(session, camera, Bank(ParticleFile::Track), elapsed);
}

void RaceParticles::Update(const sim::RaceSession& session,
                           std::span<const std::optional<SkidWheels>> skidding,
                           render::FloatMilliseconds elapsed) {
    const std::span<const sim::Racer> racers = session.Racers();
    players_.resize(skidding.size());
    computers_.resize(racers.size());
    for (std::size_t slot = 0; slot < racers.size(); ++slot) {
        if (slot >= player_ && slot - player_ < skidding.size()) {
            UpdatePlayer(racers[slot], skidding[slot - player_], players_[slot - player_]);
        } else {
            cars_.Update(
                racers[slot], computers_[slot],
                {.velocity = ToEngine(racers[slot].Body().motion.velocity) * kVelocityScale},
                Bank(ParticleFile::Common));
        }
    }
    for (render::ParticleBank& bank : banks_) {
        bank.Update(elapsed);
    }
}

void RaceParticles::UpdatePlayer(const sim::Racer& racer, const std::optional<SkidWheels>& skidding,
                                 PlayerEmitters& emitters) {
    const sim::BodyState& body = racer.Body();
    const bool isRolling = !body.hover.isHovering && !racer.IsSpunOut() &&
                           !racer.Status().isGhost && body.derived.forwardSpeed > kMinSpeed;
    const Vector3 velocity = ToEngine(body.motion.velocity) * kVelocityScale;
    std::array<WheelEmitter, sim::kWheelCount>& wheels = emitters.wheels;
    for (std::size_t i = 0; i < wheels.size(); ++i) {
        const sim::WheelProbe& probe = body.motion.wheels[i];
        UpdateWheel(wheels[i], isRolling ? ParticleOf(probe) : std::nullopt,
                    {.position = ToEngine(probe.position), .velocity = velocity});
    }
    const bool isRearKicking = wheels[kRearRight].handle || wheels[kRearLeft].handle;
    cars_.Update(racer, emitters.puffs,
                 {.skidding = skidding,
                  .isRearKicking = isRearKicking,
                  .isPlayer = true,
                  .velocity = velocity},
                 Bank(ParticleFile::Common));
}

void RaceParticles::AppendQuads(const Transform& camera, std::vector<render::QuadDraw>& out) const {
    for (const render::ParticleBank& bank : banks_) {
        bank.AppendQuads(camera, out);
    }
}

std::optional<std::string_view> RaceParticles::ParticleOf(const sim::WheelProbe& wheel) const {
    const std::optional<std::size_t> entry = wheel.surface.tableEntry;
    if (!wheel.isInContact || !entry || *entry >= surfaces_.size()) {
        return std::nullopt;
    }
    const std::optional<std::string>& particle = surfaces_[*entry].particle;
    return particle ? std::optional<std::string_view>(*particle) : std::nullopt;
}

void RaceParticles::UpdateWheel(WheelEmitter& wheel, std::optional<std::string_view> name,
                                const render::EmitterPlacement& placement) {
    if (wheel.name == name) {
        if (wheel.handle) {
            Bank(wheel.file).Place(*wheel.handle, placement);
        }
        return;
    }
    if (wheel.handle) {
        Bank(wheel.file).Release(*wheel.handle);
    }
    wheel = {.name = name};
    if (name) {
        Start(wheel, placement);
    }
}

void RaceParticles::Start(WheelEmitter& wheel, const render::EmitterPlacement& placement) {
    for (const ParticleFile file : {ParticleFile::Common, ParticleFile::Track}) {
        if (Bank(file).HasEmitter(*wheel.name)) {
            wheel.file = file;
            wheel.handle = Bank(file).Start(*wheel.name, placement);
            return;
        }
    }
}

render::ParticleBank& RaceParticles::Bank(ParticleFile file) {
    return banks_.at(static_cast<std::size_t>(file));
}

}
