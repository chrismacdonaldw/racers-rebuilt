#include "app/texture_scroll.hpp"

#include <cmath>

namespace rr::app {
namespace {

constexpr double kSpeedScale = 65.536;
constexpr double kOffsetScale = 65'536.0;

float Wrapped(float perSecond, double elapsedMs) {
    const double speed = std::trunc(static_cast<double>(perSecond) * kSpeedScale);
    const double offset = speed * elapsedMs / kOffsetScale;
    return static_cast<float>(offset - std::floor(offset));
}

}

render::UvOffset ScrolledBy(const assets::TextureScroll& scroll, float elapsedMs) {
    const auto ms = static_cast<double>(elapsedMs);
    return {.u = Wrapped(scroll.uPerSecond, ms), .v = Wrapped(scroll.vPerSecond, ms)};
}

void ScrollDraws(std::span<const ScrolledDraws> scrolled, float elapsedMs,
                 std::span<render::MeshDraw> draws) {
    for (const ScrolledDraws& span : scrolled) {
        const render::UvOffset offset = ScrolledBy(span.scroll, elapsedMs);
        for (render::MeshDraw& draw : draws.subspan(span.draws.first, span.draws.count)) {
            draw.material.uvOffset = offset;
        }
    }
}

}
