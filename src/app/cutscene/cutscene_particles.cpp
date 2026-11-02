#include "app/cutscene/cutscene_particles.hpp"

#include <format>
#include <string>
#include <utility>

#include "app/particle_assets.hpp"
#include "app/track_space.hpp"
#include "assets/game_path.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kLibrarySlots = 3;

Result<void> CheckParticles(const assets::CutsceneEffectsAsset& effects,
                            std::span<const render::ParticleBank> banks) {
    for (const assets::CutsceneParticle& particle : effects.particles) {
        if (particle.entity) {
            return Fail(ErrorCode::InvalidData,
                        std::format("particle {} follows {}, which cutscenes don't support",
                                    particle.name, particle.entity->name));
        }
        if (!banks[particle.library].HasEmitter(particle.emitter)) {
            return Fail(ErrorCode::NotFound,
                        std::format("particle {} runs {}, which {} doesn't have", particle.name,
                                    particle.emitter, effects.particleLibraries[particle.library]));
        }
    }
    return {};
}

}

CutsceneParticles::CutsceneParticles(std::span<const assets::CutsceneParticle> particles,
                                     std::vector<render::ParticleBank> banks)
    : particles_(particles), banks_(std::move(banks)), handles_(particles.size()) {}

void CutsceneParticles::Start(std::size_t particle, const Vector3& position,
                              const assets::CutsceneAxes& axes) {
    std::optional<render::EmitterHandle>& handle = handles_.at(particle);
    if (handle) {
        return;
    }
    const assets::CutsceneParticle& effect = particles_[particle];
    const assets::CutsceneAxes& turned = effect.axes.value_or(axes);
    const Transform placed = TransformToEngine(
        {.position = effect.position.value_or(position),
         .rotation = sim::ToQuaternion(sim::OrthonormalBasis(turned.direction, turned.up))});
    handle = banks_.at(effect.library)
                 .Start(effect.emitter, {.position = placed.position, .rotation = placed.rotation});
}

void CutsceneParticles::Stop(std::size_t particle) {
    if (std::optional<render::EmitterHandle>& handle = handles_.at(particle)) {
        banks_.at(particles_[particle].library).Release(*handle);
        handle.reset();
    }
}

void CutsceneParticles::Update(Milliseconds elapsed) {
    for (render::ParticleBank& bank : banks_) {
        bank.Update(render::FloatMilliseconds(elapsed));
    }
}

void CutsceneParticles::AppendQuads(const Transform& camera,
                                    std::vector<render::QuadDraw>& out) const {
    for (const render::ParticleBank& bank : banks_) {
        bank.AppendQuads(camera, out);
    }
}

Result<std::vector<render::ParticleBank>> LoadCutsceneParticles(
    const assets::Vfs& vfs, std::string_view folder, const assets::CutsceneEffectsAsset& effects,
    const assets::NameMap<assets::MaterialAsset>& materials,
    const assets::NameMap<render::TextureId>& textures) {
    std::vector<render::ParticleBank> banks;
    for (const std::string& library : effects.particleLibraries) {
        const std::string name = assets::ToUpperAscii(library);
        Result<ParticleSources> sources = ReadParticleSources(
            vfs, {.folder = std::string(folder), .emitters = name, .animation = name});
        if (!sources) {
            return std::unexpected(std::move(sources.error()));
        }
        banks.emplace_back(
            ToParticleSetup(std::move(*sources), materials, textures, kLibrarySlots));
    }
    return CheckParticles(effects, banks).transform([&] { return std::move(banks); });
}

}
