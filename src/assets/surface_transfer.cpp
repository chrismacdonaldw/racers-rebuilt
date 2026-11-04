#include "assets/surface_transfer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

namespace rr::assets {
namespace {

constexpr float kFacingDot = 0.3F;

struct Barycentric {
    float a = 1.0F;
    float b = 0.0F;
    float c = 0.0F;
};

Barycentric ClosestOnTriangle(const Vector3& p, const std::array<Vector3, 3>& corners) {
    const auto& [a, b, c] = corners;
    const Vector3 ab = b - a;
    const Vector3 ac = c - a;
    const Vector3 ap = p - a;
    const float d1 = Dot(ab, ap);
    const float d2 = Dot(ac, ap);
    if (d1 <= 0.0F && d2 <= 0.0F) {
        return {};
    }
    const Vector3 bp = p - b;
    const float d3 = Dot(ab, bp);
    const float d4 = Dot(ac, bp);
    if (d3 >= 0.0F && d4 <= d3) {
        return {.a = 0.0F, .b = 1.0F};
    }
    const float vc = (d1 * d4) - (d3 * d2);
    if (vc <= 0.0F && d1 >= 0.0F && d3 <= 0.0F) {
        const float t = d1 / (d1 - d3);
        return {.a = 1.0F - t, .b = t};
    }
    const Vector3 cp = p - c;
    const float d5 = Dot(ab, cp);
    const float d6 = Dot(ac, cp);
    if (d6 >= 0.0F && d5 <= d6) {
        return {.a = 0.0F, .c = 1.0F};
    }
    const float vb = (d5 * d2) - (d1 * d6);
    if (vb <= 0.0F && d2 >= 0.0F && d6 <= 0.0F) {
        const float t = d2 / (d2 - d6);
        return {.a = 1.0F - t, .c = t};
    }
    const float va = (d3 * d6) - (d5 * d4);
    if (va <= 0.0F && (d4 - d3) >= 0.0F && (d5 - d6) >= 0.0F) {
        const float t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return {.a = 0.0F, .b = 1.0F - t, .c = t};
    }
    const float scale = 1.0F / (va + vb + vc);
    return {.a = va * scale, .b = vb * scale, .c = vc * scale};
}

Vector3 At(const std::array<Vector3, 3>& corners, const Barycentric& weights) {
    return (corners[0] * weights.a) + (corners[1] * weights.b) + (corners[2] * weights.c);
}

float At(const std::array<float, 3>& values, const Barycentric& weights) {
    return (values[0] * weights.a) + (values[1] * weights.b) + (values[2] * weights.c);
}

std::optional<Barycentric> OnPlane(const Vector3& p, const std::array<Vector3, 3>& corners) {
    const Vector3 ab = corners[1] - corners[0];
    const Vector3 ac = corners[2] - corners[0];
    const Vector3 ap = p - corners[0];
    const float d00 = Dot(ab, ab);
    const float d01 = Dot(ab, ac);
    const float d11 = Dot(ac, ac);
    const float d20 = Dot(ap, ab);
    const float d21 = Dot(ap, ac);
    const float denominator = (d00 * d11) - (d01 * d01);
    if (denominator <= 0.0F) {
        return std::nullopt;
    }
    const float b = ((d11 * d20) - (d01 * d21)) / denominator;
    const float c = ((d00 * d21) - (d01 * d20)) / denominator;
    return Barycentric{.a = 1.0F - b - c, .b = b, .c = c};
}

SurfacePoint Extended(const SurfacePoint& own, const SurfaceTriangle& centre, const Vector3& p) {
    const std::optional<Barycentric> weights = OnPlane(p, centre.corners);
    if (!weights) {
        return own;
    }
    const float u = std::clamp(At(centre.u, *weights), 0.0F, 1.0F);
    const float v = std::clamp(At(centre.v, *weights), 0.0F, 1.0F);
    if (std::hypot(u - own.u, v - own.v) <= kSeamGap) {
        return own;
    }
    return {.triangle = &centre, .distanceSquared = own.distanceSquared, .u = u, .v = v};
}

}

SurfaceTriangle MakeSurfaceTriangle(const std::array<Vector3, 3>& corners) {
    const Vector3 normal = Cross(corners[1] - corners[0], corners[2] - corners[0]);
    return {.corners = corners, .normal = Normalized(normal, {})};
}

SurfacePoint NearestSurfacePoint(std::span<const SurfaceTriangle> surface, const Vector3& point,
                                 const Vector3& facing) {
    SurfacePoint nearest{.distanceSquared = std::numeric_limits<float>::max()};
    Barycentric weights;
    for (const SurfaceTriangle& triangle : surface) {
        const Barycentric closest = ClosestOnTriangle(point, triangle.corners);
        const Vector3 offset = At(triangle.corners, closest) - point;
        const float penalty = Dot(triangle.normal, facing) > kFacingDot ? 0.0F : kAwayPenalty;
        const float distanceSquared = Dot(offset, offset) + (penalty * penalty);
        if (distanceSquared < nearest.distanceSquared) {
            nearest = {.triangle = &triangle, .distanceSquared = distanceSquared};
            weights = closest;
        }
    }
    if (nearest.triangle == nullptr) {
        return {};
    }
    nearest.u = At(nearest.triangle->u, weights);
    nearest.v = At(nearest.triangle->v, weights);
    return nearest;
}

std::array<SurfacePoint, 3> NearestSurfacePoints(std::span<const SurfaceTriangle> surface,
                                                 const std::array<Vector3, 3>& corners,
                                                 const Vector3& facing) {
    const Vector3 centre = (corners[0] + corners[1] + corners[2]) * (1.0F / 3.0F);
    const SurfacePoint middle = NearestSurfacePoint(surface, centre, facing);
    std::array<SurfacePoint, 3> points;
    for (std::size_t corner = 0; corner < 3; ++corner) {
        const SurfacePoint own = NearestSurfacePoint(surface, corners.at(corner), facing);
        points.at(corner) =
            middle.triangle == nullptr ? own : Extended(own, *middle.triangle, corners.at(corner));
    }
    return points;
}

}
