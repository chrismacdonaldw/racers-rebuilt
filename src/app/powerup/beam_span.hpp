#pragma once

#include <cstddef>
#include <vector>

#include "app/quad_shapes.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::app {

inline constexpr std::size_t kBeamSpanSteps = 5;

struct BeamColours {
    render::Color base;
    render::Color outward;
    render::Color inward;
};

/// Takes game space and appends engine space, shading toward `outward` only if `bulge` > 0.
void AppendBeamSpan(const Vector3& from, const Vector3& to, const Vector3& side, float bulge,
                    const BeamColours& colours, std::vector<RibbonPoint>& path);

}
