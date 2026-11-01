#pragma once

#include "sim/render_snapshot.hpp"

namespace rr::sim {

/// `alpha` is in [0, 1]. Anything new in `current` or flagged `Teleported` isn't blended.
void Interpolate(const RenderSnapshot& previous, const RenderSnapshot& current, float alpha,
                 RenderSnapshot& out);

}
