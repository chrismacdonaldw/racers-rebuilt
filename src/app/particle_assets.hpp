#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "assets/emitter_library.hpp"
#include "assets/material_animation.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/particle_bank.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// File names without their extensions, in `folder`. Without `materials` every particle is hidden.
struct ParticleFiles {
    std::string folder;
    std::string emitters;
    std::string animation;
    std::optional<std::string> materials;

    friend bool operator==(const ParticleFiles&, const ParticleFiles&) = default;
};

struct ParticleSources {
    std::vector<assets::EmitterAsset> emitters;
    assets::MaterialAnimationAsset animation;
};

[[nodiscard]] ParticleFiles CommonParticleFiles();

/// `track` is a race track folder under `GAMEDATA`, such as `RACEC0R0`.
[[nodiscard]] Result<ParticleFiles> TrackParticleFiles(const assets::Vfs& vfs,
                                                       std::string_view track);

[[nodiscard]] Result<ParticleSources> ReadParticleSources(const assets::Vfs& vfs,
                                                          const ParticleFiles& files);

/// `sources` are in game space, and the setup is in engine space.
[[nodiscard]] render::ParticleBankSetup ToParticleSetup(
    ParticleSources sources, const assets::NameMap<assets::MaterialAsset>& materials,
    const assets::NameMap<render::TextureId>& textures, std::size_t slotCount);

[[nodiscard]] Result<render::ParticleBankSetup> LoadParticleBank(render::Renderer& renderer,
                                                                 const assets::Vfs& vfs,
                                                                 assets::TrackLoader& loader,
                                                                 const ParticleFiles& files,
                                                                 std::size_t slotCount);

}
