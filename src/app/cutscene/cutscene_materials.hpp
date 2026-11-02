#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_bindings.hpp"
#include "assets/cutscene_timeline.hpp"
#include "assets/material_animation.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "assets/world_database.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// One list per world, in the order of the world's material animation names.
using WorldAnimations = std::vector<std::vector<assets::MaterialAnimationAsset>>;

[[nodiscard]] Result<WorldAnimations> LoadWorldAnimations(
    const assets::Vfs& vfs, std::string_view folder,
    std::span<const assets::WorldAsset* const> worlds);

/// Runs the `MAB` tracks that model events bind to slots of their entities' materials.
class CutsceneMaterials {
public:
    /// Fails on a binding past its lists or a material `materials` lacks, as the original does.
    [[nodiscard]] static Result<CutsceneMaterials> Create(
        const assets::CutsceneFrame& frame, std::span<const ModelBinding> models,
        const WorldAnimations& animations, const assets::NameMap<render::Material>& materials);

    /// Points model event `model`'s tracks at its entity, away from any entity they ran on before.
    void Begin(std::size_t model);
    /// Stops the tracks `model` still runs, leaving their last materials shown.
    void End(std::size_t model);
    void Advance(Milliseconds elapsed);

    /// `slots[i]` is the material slot of `draws[i]`.
    void Apply(std::size_t model, std::span<render::MeshDraw> draws,
               std::span<const std::uint32_t> slots) const;

private:
    struct Entity {
        bool isJointed = false;
        std::size_t index = 0;

        friend bool operator==(const Entity&, const Entity&) = default;
    };

    struct Target {
        Entity entity;
        std::size_t slot = 0;
    };

    struct Track {
        assets::MaterialAnimationAsset animation;
        std::vector<render::Material> materials;
        Milliseconds age{0};
        std::optional<Target> target;
        std::size_t owner = 0;
    };

    struct Use {
        std::size_t track = 0;
        std::size_t slot = 0;
    };

    struct Shown {
        Target target;
        render::Material material;
    };

    CutsceneMaterials() = default;

    [[nodiscard]] Result<std::size_t> TrackFor(const assets::CutsceneMaterialBinding& binding,
                                               const WorldAnimations& animations,
                                               const assets::NameMap<render::Material>& materials);
    void Show(const Target& target, const render::Material& material);

    std::vector<Track> tracks_;
    std::vector<assets::CutsceneMaterialBinding> trackSources_;
    std::vector<std::optional<Entity>> entities_;
    std::vector<std::vector<Use>> uses_;
    std::vector<Shown> shown_;
};

}
