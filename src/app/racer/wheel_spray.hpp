#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "assets/surface_table.hpp"
#include "render/fx_system.hpp"
#include "sim/physics/racer.hpp"

namespace rr::app {

/// Rolling on its wheels, not hovering, spun out or a ghost.
[[nodiscard]] bool IsKickingUp(const sim::Racer& racer);

/// The surface table entry under `wheel`, if it touches one in the table.
[[nodiscard]] std::optional<std::size_t> SurfaceEntryOf(
    const sim::WheelProbe& wheel, std::span<const assets::SurfaceEntry> surfaces);

/// The particle the surface under `wheel` names, if it touches one.
[[nodiscard]] std::optional<std::string_view> WheelParticleOf(
    const sim::WheelProbe& wheel, std::span<const assets::SurfaceEntry> surfaces);

/// Sprays surface effects from computer cars' rear wheels, which the original leaves bare.
class WheelSpray {
public:
    /// `surfaces` must outlive it.
    explicit WheelSpray(std::span<const assets::SurfaceEntry> surfaces) : surfaces_(surfaces) {}

    void Update(std::size_t slot, const sim::Racer& racer, render::FxSystem& fx, float elapsedMs);

private:
    struct Wheel {
        std::optional<render::FxEffectId> effect;
        render::FxEmission emission;
    };

    std::span<const assets::SurfaceEntry> surfaces_;
    std::optional<std::vector<std::optional<render::FxEffectId>>> effects_;
    std::vector<std::array<Wheel, 2>> cars_;
};

}
