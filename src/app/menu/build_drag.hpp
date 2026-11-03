#pragma once

#include <cstdint>
#include <optional>

#include "app/menu/car_build_menu.hpp"
#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "sim/input_frame.hpp"

namespace rr::app {

enum class BuildRegion : std::uint8_t { Piece, Car };

struct BuildRegions {
    render::Rect scene;
    std::optional<render::Rect> piece;
    std::optional<render::Rect> car;
};

struct SceneMouse {
    BuildRequest request;
    float turn = 0.0F;
    float tilt = 0.0F;
};

/// `sphere` is in engine space and `view` is the viewport on the UI canvas.
[[nodiscard]] std::optional<render::Rect> SphereOnScreen(const render::Camera& camera,
                                                         const render::Rect& view,
                                                         const BoundingSphere& sphere);

/// A press on the piece or car holds the move or view pad until any button is released.
class BuildDrag {
public:
    /// `regions` are read only while no pad is held, as the pointer is hidden then.
    [[nodiscard]] SceneMouse Tick(const sim::MenuPointer& pointer, const BuildRegions& regions,
                                  CarBuildMenu& menu);

    [[nodiscard]] std::optional<BuildRegion> Lit() const { return lit_; }

private:
    [[nodiscard]] BuildRequest Press(CarBuildMenu& menu);
    [[nodiscard]] BuildRequest DragPiece(const sim::MenuPointer& pointer);

    std::optional<BuildRegion> lit_;
    Milliseconds doubleClickLeft_{0};
    Milliseconds dragDelay_{0};
    float dragX_ = 0.0F;
    float dragY_ = 0.0F;
};

}
