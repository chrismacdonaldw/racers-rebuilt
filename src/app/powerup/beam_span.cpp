#include "app/powerup/beam_span.hpp"

#include "app/track_space.hpp"

namespace rr::app {
namespace {

render::Color Mix(const render::Color& from, const render::Color& to, float share) {
    return {.r = from.r + ((to.r - from.r) * share),
            .g = from.g + ((to.g - from.g) * share),
            .b = from.b + ((to.b - from.b) * share),
            .a = from.a + ((to.a - from.a) * share)};
}

}

void AppendBeamSpan(const Vector3& from, const Vector3& to, const Vector3& side, float bulge,
                    const BeamColours& colours, std::vector<RibbonPoint>& path) {
    const Vector3 control = Lerp(from, to, 0.5F) + (side * bulge);
    const render::Color& peak = bulge > 0.0F ? colours.outward : colours.inward;
    for (std::size_t step = 1; step <= kBeamSpanSteps; ++step) {
        const float share = static_cast<float>(step) / static_cast<float>(kBeamSpanSteps);
        const Vector3 point = Lerp(Lerp(from, control, share), Lerp(control, to, share), share);
        path.push_back({.position = ToEngine(point),
                        .tint = Mix(colours.base, peak, 2.0F * share * (1.0F - share))});
    }
}

}
