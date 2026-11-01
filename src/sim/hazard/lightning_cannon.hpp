#pragma once

#include <cstddef>
#include <optional>

#include "assets/hazard_list.hpp"
#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "sim/hazard/hazard_context.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::sim {

inline constexpr Milliseconds kLightningCannonCooldown{8300};

/// Sweeps from `entity`'s root when it has one, else from `source`.
struct LightningCannon {
    std::optional<std::size_t> entity;
    Vector3 source;
    Milliseconds period{0};
    float sweepRadians = 0.0F;
    float verticalRange = 0.0F;
    float verticalBase = 0.0F;
    Milliseconds sweepTime{0};
    Milliseconds cooldown{0};

    void Activate() { cooldown = kLightningCannonCooldown; }
    void Tick(HazardContext& context);
};

[[nodiscard]] LightningCannon LoadLightningCannon(const assets::LightningCannon& payload,
                                                  const TrackEntities& entities);

}
