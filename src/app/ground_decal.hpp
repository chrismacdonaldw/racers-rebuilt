#pragma once

#include <cstdint>
#include <vector>

#include "app/decal_surface.hpp"
#include "app/quad_shapes.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::app {

inline constexpr float kDecalCastAbove = 6.0F;

/// In game space, with `length` along the level part of `forward`.
struct GroundDecal {
    Vector3 centre;
    Vector3 forward;
    float length = 0.0F;
    float width = 0.0F;
};

/// Casts decals straight down onto `surface`, clipping each triangle they cover as the original.
class GroundDecals {
public:
    explicit GroundDecals(const DecalSurface& surface) : surface_(&surface) {}

    void Append(const QuadLook& look, const GroundDecal& decal, std::vector<render::QuadDraw>& out);

private:
    const DecalSurface* surface_;
    std::vector<std::uint32_t> nearby_;
};

}
