#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "assets/material_animation.hpp"
#include "assets/track_loader.hpp"
#include "assets/world_database.hpp"
#include "render/render_view.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

class TrackEntityDraws;

struct MaterialSources {
    const assets::NameMap<assets::AssetCache<assets::MaterialAnimationAsset>::Handle>* animations =
        nullptr;
    const assets::NameMap<render::Material>* materials = nullptr;
};

/// Plays the `MAB` tracks that world entities bind to their material slots, looping from the start.
class TrackMaterialAnims {
public:
    /// `slots` label placed draws from `firstDraw`, none for animated; skips unplayable bindings.
    void Bind(const assets::WorldAsset& world, const assets::WorldEntity& entity,
              std::span<const std::uint32_t> slots, std::size_t firstDraw,
              const MaterialSources& sources);

    void FindEntities(const sim::TrackEntitySources& sources, const TrackEntityDraws& draws);

    /// Animated entities' draws start at `entityFirst` in `draws`, after the track's own.
    void Apply(float shownMs, std::size_t entityFirst, std::span<render::MeshDraw> draws) const;

private:
    struct Track {
        const assets::WorldAsset* world = nullptr;
        std::size_t animation = 0;
        std::size_t track = 0;
        assets::MaterialAnimationAsset frames;
        std::vector<render::Material> materials;
        std::string entity;
        std::uint32_t slot = 0;
        std::vector<std::size_t> trackDraws;
        std::vector<std::size_t> entityDraws;
    };

    std::vector<Track> tracks_;
};

}
