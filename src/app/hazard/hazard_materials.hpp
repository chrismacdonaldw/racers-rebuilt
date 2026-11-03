#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "assets/material_animation.hpp"
#include "render/render_view.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// `frames` holds one track from key 0, and `materials` its keys' materials in order.
struct PlayedTrack {
    assets::MaterialAnimationAsset frames;
    std::vector<render::Material> materials;
};

/// A bound track a hazard plays instead of looping: a code lamp, or a sphinx loop until the hit.
struct HazardDriven {
    enum class Kind : std::uint8_t { Lamp, Sphinx };

    Kind kind = Kind::Lamp;
    std::size_t lamp = 0;
    std::size_t blowUpTrack = 0;
    PlayedTrack blowUp;
};

[[nodiscard]] std::optional<HazardDriven> FindHazardDriven(std::string_view entity,
                                                           std::size_t track);

/// Null while the bound track loops as usual.
[[nodiscard]] const render::Material* HazardShown(const HazardDriven& driven,
                                                  const PlayedTrack& bound,
                                                  const sim::HazardMaterialsSnapshot& hazards);

}
