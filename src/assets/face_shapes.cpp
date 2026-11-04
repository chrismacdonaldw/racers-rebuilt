#include "assets/face_shapes.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <utility>

namespace rr::assets {
namespace {

constexpr int kSamplesPerSide = 4;
constexpr int kSamplesInside = (kSamplesPerSide * kSamplesPerSide) / 2;
constexpr int kSizePx = static_cast<int>(kFaceDrawingSizePx);
constexpr double kRadiansPerDegree = std::numbers::pi / 180.0;

struct PixelBox {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

PixelBox BoxAround(FacePoint low, FacePoint high) {
    return {.x0 = std::max(static_cast<int>(std::floor(low[0])) - 1, 0),
            .y0 = std::max(static_cast<int>(std::floor(low[1])) - 1, 0),
            .x1 = std::min(static_cast<int>(std::ceil(high[0])) + 1, kSizePx),
            .y1 = std::min(static_cast<int>(std::ceil(high[1])) + 1, kSizePx)};
}

PixelBox BoxOf(std::span<const FacePoint> points, double pad) {
    FacePoint low{points.front()};
    FacePoint high{points.front()};
    for (const FacePoint& point : points) {
        for (std::size_t axis = 0; axis < point.size(); ++axis) {
            low.at(axis) = std::min(low.at(axis), point.at(axis));
            high.at(axis) = std::max(high.at(axis), point.at(axis));
        }
    }
    return BoxAround({low[0] - pad, low[1] - pad}, {high[0] + pad, high[1] + pad});
}

double SampleAt(int pixel, int sample) {
    return static_cast<double>(pixel) + ((static_cast<double>(sample) + 0.5) / kSamplesPerSide);
}

template <typename Inside>
std::vector<std::uint8_t> MaskOf(const PixelBox& box, const Inside& isInside) {
    std::vector<std::uint8_t> mask(std::size_t{kFaceDrawingSizePx} * kFaceDrawingSizePx);
    for (int y = box.y0; y < box.y1; ++y) {
        for (int x = box.x0; x < box.x1; ++x) {
            int count = 0;
            for (int sy = 0; sy < kSamplesPerSide; ++sy) {
                for (int sx = 0; sx < kSamplesPerSide; ++sx) {
                    count += isInside(SampleAt(x, sx), SampleAt(y, sy)) ? 1 : 0;
                }
            }
            mask[(static_cast<std::size_t>(y) * kFaceDrawingSizePx) + static_cast<std::size_t>(x)] =
                count >= kSamplesInside ? 1 : 0;
        }
    }
    return mask;
}

int WindingAt(std::span<const FacePoint> points, double x, double y) {
    int winding = 0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto [x0, y0] = points[i];
        const auto [x1, y1] = points[(i + 1) % points.size()];
        const double cross = ((x1 - x0) * (y - y0)) - ((x - x0) * (y1 - y0));
        if (y0 <= y && y1 > y && cross > 0.0) {
            ++winding;
        } else if (y0 > y && y1 <= y && cross < 0.0) {
            --winding;
        }
    }
    return winding;
}

bool IsNearSegment(const FacePoint& a, const FacePoint& b, double radiusSquared, double x,
                   double y) {
    const double dx = b[0] - a[0];
    const double dy = b[1] - a[1];
    const double lengthSquared = (dx * dx) + (dy * dy);
    const double t =
        lengthSquared > 0.0
            ? std::clamp((((x - a[0]) * dx) + ((y - a[1]) * dy)) / lengthSquared, 0.0, 1.0)
            : 0.0;
    const double ox = x - a[0] - (t * dx);
    const double oy = y - a[1] - (t * dy);
    return (ox * ox) + (oy * oy) <= radiusSquared;
}

std::vector<std::uint8_t> MaskOfShape(const FacePolygon& polygon) {
    return MaskOf(BoxOf(polygon.points, 0.0),
                  [&](double x, double y) { return WindingAt(polygon.points, x, y) != 0; });
}

std::vector<std::uint8_t> MaskOfShape(const FaceLine& line) {
    const double radius = line.width / 2.0;
    const double radiusSquared = radius * radius;
    return MaskOf(BoxOf(line.points, radius), [&](double x, double y) {
        if (line.points.size() == 1) {
            return IsNearSegment(line.points[0], line.points[0], radiusSquared, x, y);
        }
        for (std::size_t i = 0; i + 1 < line.points.size(); ++i) {
            if (IsNearSegment(line.points[i], line.points[i + 1], radiusSquared, x, y)) {
                return true;
            }
        }
        return false;
    });
}

std::vector<std::uint8_t> MaskOfShape(const FaceEllipse& ellipse) {
    const auto [cx, cy] = ellipse.centre;
    const auto [rx, ry] = ellipse.radii;
    const double reach = std::max(rx, ry);
    const double angle = ellipse.degrees * kRadiansPerDegree;
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    return MaskOf(BoxAround({cx - reach, cy - reach}, {cx + reach, cy + reach}),
                  [&](double x, double y) {
                      const double u = ((x - cx) * cosine) + ((y - cy) * sine);
                      const double v = (-(x - cx) * sine) + ((y - cy) * cosine);
                      return ((u / rx) * (u / rx)) + ((v / ry) * (v / ry)) <= 1.0;
                  });
}

std::vector<std::uint8_t> MaskOfShape(const FaceEverywhere& /*everywhere*/) {
    return std::vector<std::uint8_t>(std::size_t{kFaceDrawingSizePx} * kFaceDrawingSizePx, 1);
}

}

std::vector<std::uint8_t> ShapeMask(const FaceShape& shape) {
    return std::visit([](const auto& kind) { return MaskOfShape(kind); }, shape);
}

}
