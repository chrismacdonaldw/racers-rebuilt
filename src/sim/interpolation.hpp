#pragma once

#include "sim/render_snapshot.hpp"

namespace rr::sim {

/// `alpha` is in [0, 1]. Entities new in `current` or flagged `Teleported` aren't blended.
void Interpolate(const RenderSnapshot& previous, const RenderSnapshot& current, float alpha,
                 RenderSnapshot& out);

}
