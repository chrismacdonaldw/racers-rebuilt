#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "app/track_model.hpp"
#include "assets/hazard_list.hpp"
#include "core/duration.hpp"
#include "render/render_view.hpp"

namespace rr::app {

class WaterScroll {
public:
    WaterScroll(std::span<const assets::Hazard> hazards, std::span<const NamedDraws> named);

    void Tick(Milliseconds elapsed);

    /// `draws` starts with the track draws.
    void Apply(std::span<render::MeshDraw> draws) const;

private:
    struct Surface {
        std::vector<DrawSpan> spans;
        float uAmplitude = 0.0F;
        float vAmplitude = 0.0F;
        std::int16_t uSpeed = 0;
        std::int16_t vSpeed = 0;
        std::uint16_t u = 0;
        std::uint16_t v = 0;
    };

    std::vector<Surface> surfaces_;
    Milliseconds phase_{0};
};

}
