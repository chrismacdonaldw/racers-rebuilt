#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>

#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "sim/input_frame.hpp"
#include "sim/powerup/warp_effect.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

[[nodiscard]] const sim::EffectSnapshot* FindWarp(std::span<const sim::EffectSnapshot> effects,
                                                  std::size_t racer, sim::WarpPhase phase);

/// Opens from nothing and closes again over the start, widest halfway through.
[[nodiscard]] float WarpPortalScale(float leftMs);

/// Shrinks the car with the closing portal over the start's last 250 ms.
[[nodiscard]] float WarpCarScale(float leftMs);

/// Eases the field of view 30 degrees wider over the start.
[[nodiscard]] render::Camera WarpStartCamera(render::Camera camera, float leftMs);

/// Fades the whole screen to dark blue over the start's last 200 ms.
[[nodiscard]] std::optional<render::SpriteDraw> WarpFlash(float leftMs);

/// Player `i` drives racer `firstRacer + i`.
[[nodiscard]] std::array<std::optional<render::SpriteDraw>, sim::kMaxPlayers> WarpFlashes(
    std::span<const sim::EffectSnapshot> effects, std::size_t firstRacer, std::size_t players);

}
