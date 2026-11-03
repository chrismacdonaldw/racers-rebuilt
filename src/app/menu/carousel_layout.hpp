#pragma once

#include <cstddef>
#include <vector>

#include "assets/menu_layout.hpp"
#include "core/bounding_sphere.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"

namespace rr::app {

struct CarouselSlot {
    float across = 0.0F;
    float fit = 0.0F;
};

/// Its camera sits `distance` along game +X, looking back at the origin with +Z up.
struct CarouselLayout {
    render::Rect bounds;
    float fovDegrees = 0.0F;
    float distance = 0.0F;
    float farPlane = 0.0F;
    std::vector<CarouselSlot> slots;
    std::size_t focused = 0;
};

/// `bounds` is the carousel's rectangle on the UI canvas, which its slots are relative to.
[[nodiscard]] Result<CarouselLayout> LayoutCarousel(const assets::ModelCarouselElement& carousel,
                                                    const render::Rect& bounds);

/// In game space, as the original places a model with these bounds in the slot.
[[nodiscard]] Vector3 SlotPosition(const CarouselLayout& layout, std::size_t slot,
                                   const BoundingSphere& bounds);

[[nodiscard]] render::Camera CarouselCamera(const CarouselLayout& layout);

}
