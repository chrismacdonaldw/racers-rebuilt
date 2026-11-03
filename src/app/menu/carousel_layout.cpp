#include "app/menu/carousel_layout.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "app/track_space.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

constexpr std::size_t kFovValue = 6;
constexpr std::size_t kDistanceValue = 7;
constexpr std::size_t kFarValue = 8;
constexpr float kSpareDepth = 2.0F;
constexpr Vector3 kLookBack{.x = -1.0F};
constexpr Vector3 kUp{.z = 1.0F};

}

Result<CarouselLayout> LayoutCarousel(const assets::ModelCarouselElement& carousel,
                                      const render::Rect& bounds) {
    if (!carousel.camera || carousel.camera->at(kDistanceValue) <= 0.0F || bounds.width <= 0.0F ||
        bounds.height <= 0.0F || carousel.focusedSlot < 0 ||
        std::cmp_greater_equal(carousel.focusedSlot, carousel.slots.size())) {
        return Fail(ErrorCode::InvalidData, "the model carousel has no camera, size or focus");
    }
    CarouselLayout layout{.bounds = bounds,
                          .fovDegrees = carousel.camera->at(kFovValue),
                          .distance = carousel.camera->at(kDistanceValue),
                          .farPlane = carousel.camera->at(kFarValue),
                          .focused = static_cast<std::size_t>(carousel.focusedSlot)};
    const float halfFov = layout.fovDegrees * std::numbers::pi_v<float> / 360.0F;
    const float unit = 2.0F * layout.distance * std::tan(halfFov) / bounds.height;
    for (const assets::MenuRect& slot : carousel.slots) {
        const float left = (static_cast<float>(slot.left) - (bounds.width * 0.5F)) * unit;
        const float width = static_cast<float>(slot.right - slot.left) * unit;
        const float height = static_cast<float>(slot.bottom - slot.top) * unit;
        layout.slots.push_back({.across = left + (width * 0.5F), .fit = std::min(width, height)});
    }
    return layout;
}

Vector3 SlotPosition(const CarouselLayout& layout, std::size_t slot, const BoundingSphere& bounds) {
    const CarouselSlot& place = layout.slots.at(slot);
    const float diameter = 2.0F * bounds.radius;
    float depth = diameter - place.fit;
    if (depth > 0.0F && place.fit > 0.0F) {
        depth = -(layout.distance / place.fit) * depth;
    }
    depth = std::min(depth, -(diameter + kSpareDepth));
    const float spread = (layout.distance - depth) / layout.distance;
    return Vector3{.x = depth, .y = spread * place.across} - bounds.centre;
}

render::Camera CarouselCamera(const CarouselLayout& layout) {
    const Transform view{.position = {.x = layout.distance},
                         .rotation = sim::ToQuaternion(sim::OrthonormalBasis(kLookBack, kUp))};
    return {.transform = ViewToEngine(view),
            .verticalFovDegrees = layout.fovDegrees,
            .nearPlane = layout.distance,
            .farPlane = layout.farPlane};
}

}
