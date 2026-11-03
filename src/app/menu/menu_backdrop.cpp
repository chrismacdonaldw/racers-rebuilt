#include "app/menu/menu_backdrop.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

#include "app/menu/menu_page.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr float kFarShare = 0.99F;

std::optional<render::Rect> Overlap(const render::Rect& a, const render::Rect& b) {
    const float left = std::max(a.x, b.x);
    const float top = std::max(a.y, b.y);
    const float right = std::min(a.x + a.width, b.x + b.width);
    const float bottom = std::min(a.y + a.height, b.y + b.height);
    if (right <= left || bottom <= top) {
        return std::nullopt;
    }
    return render::Rect{.x = left, .y = top, .width = right - left, .height = bottom - top};
}

class FarPlane {
public:
    FarPlane(const render::Rect& hole, const render::Camera& camera) : hole_(hole) {
        const float distance = camera.farPlane * kFarShare;
        const float halfFov = camera.verticalFovDegrees * std::numbers::pi_v<float> / 360.0F;
        const float halfHeight = distance * std::tan(halfFov);
        const Quaternion& turn = camera.transform.rotation;
        centre_ = camera.transform.position + Rotate(turn, {.z = -distance});
        right_ = Rotate(turn, {.x = halfHeight * hole.width / hole.height});
        up_ = Rotate(turn, {.y = halfHeight});
    }

    [[nodiscard]] Vector3 At(float x, float y) const {
        const float across = (2.0F * (x - hole_.x) / hole_.width) - 1.0F;
        const float down = (2.0F * (y - hole_.y) / hole_.height) - 1.0F;
        return centre_ + (right_ * across) - (up_ * down);
    }

private:
    render::Rect hole_;
    Vector3 centre_;
    Vector3 right_;
    Vector3 up_;
};

}

void AppendBackdropQuads(std::span<const MenuPicture> pictures, const render::Rect& hole,
                         const render::Camera& camera, std::vector<render::QuadDraw>& out) {
    if (hole.width <= 0.0F || hole.height <= 0.0F) {
        return;
    }
    const FarPlane plane(hole, camera);
    for (const MenuPicture& picture : pictures) {
        const std::optional<render::Rect> piece = Overlap(picture.bounds, hole);
        if (!piece) {
            continue;
        }
        const float right = piece->x + piece->width;
        const float bottom = piece->y + piece->height;
        out.push_back({.texture = picture.texture,
                       .corners = {plane.At(piece->x, piece->y), plane.At(right, piece->y),
                                   plane.At(piece->x, bottom), plane.At(right, bottom)},
                       .uv = PieceUv(picture.bounds, *piece),
                       .tint = picture.tint});
    }
}

}
