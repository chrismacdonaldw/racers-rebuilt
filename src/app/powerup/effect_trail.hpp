#pragma once

#include <vector>

#include "app/quad_shapes.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// `width` is the side of the square tube in game units.
struct TrailStyle {
    float width = 0.0F;
    float lifeMs = 0.0F;
    render::Color colour;
};

class EffectTrail {
public:
    /// In game space.
    void Follow(const Vector3& head, const TrailStyle& style, float elapsedMs);
    void Clear();

    void Append(const QuadLook& look, const TrailStyle& style,
                std::vector<render::QuadDraw>& out) const;

private:
    struct Point {
        Vector3 position;
        float ageMs = 0.0F;
    };

    std::vector<Point> points_;
    float sinceSampleMs_ = 0.0F;
};

}
