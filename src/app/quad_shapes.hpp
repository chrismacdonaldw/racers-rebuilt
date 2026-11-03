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
    render::TextureSampling sampling;
};

/// Quads never write depth, so an opaque material blends by its texture's alpha instead.
[[nodiscard]] QuadLook LookOf(const render::Material& material);

[[nodiscard]] render::TextureSampling SamplingOf(const render::Material& material);

/// In engine space, `width` along the camera's right axis and `height` along its up axis.
void AppendBillboard(const QuadLook& look, const Vector3& centre, float width, float height,
                     const Transform& camera, std::vector<render::QuadDraw>& out);

/// In engine space. `top` and `right` run from the centre to the texture's top and right edges.
void AppendGroundQuad(const QuadLook& look, const Vector3& centre, const Vector3& normal,
                      const Vector3& top, const Vector3& right, std::vector<render::QuadDraw>& out);

/// A level square in engine space, lifted clear of the ground it lies on.
void AppendDecal(const QuadLook& look, const Vector3& centre, float size,
                 std::vector<render::QuadDraw>& out);

struct RibbonPoint {
    Vector3 position;
    render::Color tint;
};

/// A camera-facing strip through `points` in engine space, tinted by each point's `tint`.
void AppendRibbon(const QuadLook& look, std::span<const RibbonPoint> points, float width,
                  const Transform& camera, std::vector<render::QuadDraw>& out);

}
