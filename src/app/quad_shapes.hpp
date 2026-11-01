#pragma once

#include <span>
#include <vector>

#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::app {

struct QuadLook {
    render::TextureId texture = render::kWhiteTexture;
    render::Color tint;
    render::BlendMode blend = render::BlendMode::Alpha;
};

/// Quads never write depth, so an opaque material blends by its texture's alpha instead.
[[nodiscard]] QuadLook LookOf(const render::Material& material);

[[nodiscard]] render::Color Modulate(const render::Color& a, const render::Color& b);

/// In engine space, `width` along the camera's right axis and `height` along its up axis.
void AppendBillboard(const QuadLook& look, const Vector3& centre, float width, float height,
                     const Transform& camera, std::vector<render::QuadDraw>& out);

/// A level square in engine space, lifted clear of the ground it lies on.
void AppendDecal(const QuadLook& look, const Vector3& centre, float size,
                 std::vector<render::QuadDraw>& out);

/// A strip through `points` in engine space, each segment turned to face the camera.
void AppendRibbon(const QuadLook& look, std::span<const Vector3> points, float width,
                  const Transform& camera, std::vector<render::QuadDraw>& out);

}
