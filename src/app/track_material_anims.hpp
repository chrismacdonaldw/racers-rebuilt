#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "app/hazard/hazard_materials.hpp"
#include "assets/track_loader.hpp"
#include "assets/world_database.hpp"
#include "render/render_view.hpp"
#include "sim/render_snapshot.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

class TrackEntityDraws;

struct MaterialSources {
    const assets::NameMap<assets::AssetCache<assets::MaterialAnimationAsset>::Handle>* animations =
        nullptr;
    const assets::NameMap<render::Material>* materials = nullptr;
};

/// Nothing when the world's animation, that track or one of its key materials is missing.
[[nodiscard]] std::optional<PlayedTrack> ResolveTrack(const assets::WorldAsset& world,
                                                      std::size_t animation, std::size_t track,
                                                      const MaterialSources& sources);

/// Loops the `MAB` tracks world entities bind to their material slots, unless a hazard drives them.
class TrackMaterialAnims {
public:
    /// `slots` label placed draws from `firstDraw`, none for animated; skips unplayable bindings.
    void Bind(const assets::WorldAsset& world, const assets::WorldEntity& entity,
              std::span<const std::uint32_t> slots, std::size_t firstDraw,
              const MaterialSources& sources);

    void FindEntities(const sim::TrackEntitySources& sources, const TrackEntityDraws& draws);

    /// Animated entities' draws start at `entityFirst` in `draws`, after the track's own.
    void Apply(float shownMs, const sim::HazardMaterialsSnapshot& hazards, std::size_t entityFirst,
               std::span<render::MeshDraw> draws) const;

private:
    struct Track {
        const assets::WorldAsset* world = nullptr;
        std::size_t animation = 0;
        std::size_t track = 0;
        PlayedTrack played;
        std::optional<HazardDriven> driven;
        std::string entity;
        std::uint32_t slot = 0;
        std::vector<std::size_t> trackDraws;
        std::vector<std::size_t> entityDraws;
    };

    std::vector<Track> tracks_;
};

}
