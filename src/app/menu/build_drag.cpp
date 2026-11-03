#include "app/menu/build_drag.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "app/frame_loop.hpp"

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>

namespace rr::app {
namespace {

constexpr Milliseconds kDoubleClick{500};
constexpr Milliseconds kDragDelay{100};
constexpr float kDragThreshold = 5.0F;
constexpr float kSwingPerMove = 0.01F;

bool Contains(const render::Rect& area, const sim::MenuPointer& pointer) {
    return pointer.x >= area.x && pointer.y >= area.y && pointer.x <= area.x + area.width &&
           pointer.y <= area.y + area.height;
}

std::optional<BuildRegion> RegionAt(const BuildRegions& regions, const sim::MenuPointer& pointer) {
    if (!pointer.isOver || !Contains(regions.scene, pointer)) {
        return std::nullopt;
    }
    if (regions.piece && Contains(*regions.piece, pointer)) {
        return BuildRegion::Piece;
    }
    if (regions.car && Contains(*regions.car, pointer)) {
        return BuildRegion::Car;
    }
    return std::nullopt;
}

int Across(float dy) {
    if (dy >= kDragThreshold) {
        return 4;
    }
    return dy > -kDragThreshold ? 3 : 2;
}

int Back(float dy) {
    if (dy >= kDragThreshold) {
        return 6;
    }
    return dy <= -kDragThreshold ? 8 : 7;
}

int DragSpot(float dx, float dy) {
    if (dx >= kDragThreshold) {
        return Across(dy);
    }
    if (dx <= -kDragThreshold) {
        return Back(dy);
    }
    if (dy >= kDragThreshold) {
        return 5;
    }
    return dy <= -kDragThreshold ? 1 : 0;
}

Milliseconds CountDown(Milliseconds left) {
    return std::max(left - kSimTick, Milliseconds{0});
}

}

std::optional<render::Rect> SphereOnScreen(const render::Camera& camera, const render::Rect& view,
                                           const BoundingSphere& sphere) {
    const glm::vec4 eye = glm::inverse(render::ModelMatrix(camera.transform)) *
                          glm::vec4(sphere.centre.x, sphere.centre.y, sphere.centre.z, 1.0F);
    const float depth = -eye.z;
    if (depth <= sphere.radius || view.width <= 0.0F || view.height <= 0.0F) {
        return std::nullopt;
    }
    const float focal = 1.0F / std::tan(glm::radians(camera.verticalFovDegrees) / 2.0F);
    const float halfHeight = view.height / 2.0F;
    const float scale = focal * halfHeight / depth;
    const float x = view.x + (view.width / 2.0F) + (eye.x * scale);
    const float y = view.y + halfHeight - (eye.y * scale);
    const float reach = sphere.radius * scale;
    return render::Rect{
        .x = x - reach, .y = y - reach, .width = 2.0F * reach, .height = 2.0F * reach};
}

SceneMouse BuildDrag::Tick(const sim::MenuPointer& pointer, const BuildRegions& regions,
                           CarBuildMenu& menu) {
    doubleClickLeft_ = CountDown(doubleClickLeft_);
    dragDelay_ = CountDown(dragDelay_);
    const std::optional<BuildPad> held = menu.Held();
    if (!held) {
        lit_ = RegionAt(regions, pointer);
    }
    SceneMouse mouse;
    if (pointer.rightPressed) {
        if (lit_ == BuildRegion::Piece) {
            mouse.request = {.command = BuildCommand::Rotate};
        }
    } else if (pointer.pressed) {
        mouse.request = Press(menu);
    } else if (held == BuildPad::Move) {
        mouse.request = DragPiece(pointer);
    } else if (held == BuildPad::View) {
        mouse.turn = -pointer.dx * kSwingPerMove;
        mouse.tilt = pointer.dy * kSwingPerMove;
    }
    if (pointer.released && menu.Held()) {
        menu.LetGo();
    }
    return mouse;
}

BuildRequest BuildDrag::Press(CarBuildMenu& menu) {
    if (lit_ == BuildRegion::Car) {
        menu.Hold(BuildPad::View);
        return {};
    }
    if (lit_ != BuildRegion::Piece) {
        return {};
    }
    if (doubleClickLeft_ > Milliseconds{0} && !menu.Held()) {
        doubleClickLeft_ = Milliseconds{0};
        return {.command = BuildCommand::Place};
    }
    menu.Hold(BuildPad::Move);
    doubleClickLeft_ = kDoubleClick;
    dragX_ = 0.0F;
    dragY_ = 0.0F;
    return {};
}

BuildRequest BuildDrag::DragPiece(const sim::MenuPointer& pointer) {
    if (dragDelay_ > Milliseconds{0}) {
        dragX_ = 0.0F;
        dragY_ = 0.0F;
        return {};
    }
    dragX_ += pointer.dx;
    dragY_ += pointer.dy;
    const int spot = DragSpot(dragX_, dragY_);
    if (spot == 0) {
        return {};
    }
    dragX_ = 0.0F;
    dragY_ = 0.0F;
    dragDelay_ = kDragDelay;
    return {.command = BuildCommand::Move, .direction = static_cast<std::size_t>(spot - 1)};
}

}
