#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "assets/cutscene_effects.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "render/particle_bank.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// Runs the `CEB` particles, one bank per library. `particles` must outlive it.
class CutsceneParticles {
public:
    CutsceneParticles(std::span<const assets::CutsceneParticle> particles,
                      std::vector<render::ParticleBank> banks);

    /// Spawns once until a stop. The particle's own position and axes win over these game ones.
    void Start(std::size_t particle, const Vector3& position, const assets::CutsceneAxes& axes);

    /// Removes its particles at once, as the original's release does.
    void Stop(std::size_t particle);

    void Update(Milliseconds elapsed);
    void AppendQuads(const Transform& camera, std::vector<render::QuadDraw>& out) const;

private:
    std::span<const assets::CutsceneParticle> particles_;
    std::vector<render::ParticleBank> banks_;
    std::vector<std::optional<render::EmitterHandle>> handles_;
};

/// Fails on an emitter its library lacks and on a particle that follows an entity.
[[nodiscard]] Result<std::vector<render::ParticleBank>> LoadCutsceneParticles(
    const assets::Vfs& vfs, std::string_view folder, const assets::CutsceneEffectsAsset& effects,
    const assets::NameMap<assets::MaterialAsset>& materials,
    const assets::NameMap<render::TextureId>& textures);

}
