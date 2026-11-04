#include "assets/ldraw_head.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "assets/face_drawing.hpp"
#include "assets/ldraw_part.hpp"
#include "assets/ldraw_prints.hpp"
#include "core/vector3.hpp"

namespace rr::assets {
namespace {

constexpr double kCanvasWidth = 2400.0;
constexpr double kCanvasHeight = 1600.0;
constexpr double kHalfTurnDegrees = 180.0;
constexpr double kQuarterTurnDegrees = 90.0;
constexpr double kDegreesPerRadian = 180.0 / std::numbers::pi;
constexpr std::array kSideU{0.023, 0.28, 0.72, 0.977};
constexpr std::array kSideDegrees{21.5, 66.5, 111.5, 156.5};
constexpr double kCentreDegrees = 89.0;
constexpr double kRowTop = 0.023;
constexpr double kRowBottom = 0.899;
constexpr double kLastRowFraction = 0.9999;
constexpr float kSideDepth = 0.6F * kGameUnitsPerLdu;
constexpr float kMaxSideSlope = 0.3F;
constexpr std::size_t kBytesPerPixel = 4;
constexpr Rgba8 kYellow{0xFA, 0xC8, 0x0A, 0xFF};
constexpr std::string_view kMainColour = "16";

using CanvasPoint = std::array<double, 2>;

struct SideTriangle {
    bool isMain = false;
    Rgba8 rgba{};
    std::array<CanvasPoint, 3> corners{};
};

double PhiOfU(double u) {
    u = std::clamp(u, kSideU.front(), kSideU.back());
    for (std::size_t side = 0; side + 1 < kSideU.size(); ++side) {
        if (u <= kSideU.at(side + 1)) {
            const double t = (u - kSideU.at(side)) / (kSideU.at(side + 1) - kSideU.at(side));
            const double a = kSideDegrees.at(side) / kDegreesPerRadian;
            const double b = kSideDegrees.at(side + 1) / kDegreesPerRadian;
            const double x = (std::cos(a) * (1.0 - t)) + (std::cos(b) * t);
            const double y = (std::sin(a) * (1.0 - t)) + (std::sin(b) * t);
            return (std::atan2(y, x) * kDegreesPerRadian) - kCentreDegrees;
        }
    }
    return kSideDegrees.back() - kCentreDegrees;
}

double PhiOf(const Vector3& position) {
    return std::atan2(static_cast<double>(position.x), -static_cast<double>(position.y)) *
           kDegreesPerRadian;
}

float RadiusOf(const Vector3& position) {
    return std::hypot(position.x, position.y);
}

bool IsOnSide(const std::array<Vector3, 3>& corners, float headRadius) {
    if (std::ranges::any_of(corners, [&](const Vector3& corner) {
            return RadiusOf(corner) < headRadius - kSideDepth;
        })) {
        return false;
    }
    const Vector3 normal = Cross(corners[1] - corners[0], corners[2] - corners[0]);
    const float length = Length(normal);
    if (length == 0.0F || std::abs(normal.z) > kMaxSideSlope * length) {
        return false;
    }
    const auto [low, high] = std::ranges::minmax(corners | std::views::transform(PhiOf));
    return high - low <= kHalfTurnDegrees;
}

std::vector<SideTriangle> SideTrianglesOf(const GeometryAsset& head) {
    float headRadius = 0.0F;
    for (const GeometryVertex& vertex : head.vertices) {
        headRadius = std::max(headRadius, RadiusOf(vertex.position));
    }
    std::vector<SideTriangle> triangles;
    for (const GeometryRange& range : head.ranges) {
        const std::string& code = head.materials[range.material];
        const Rgba8 rgba = LdrawColourOf(code).value_or(kYellow);
        for (std::uint32_t i = range.firstIndex; i + 2 < range.firstIndex + range.indexCount;
             i += 3) {
            const std::array corners{head.vertices[head.indices[i]].position,
                                     head.vertices[head.indices[i + 1]].position,
                                     head.vertices[head.indices[i + 2]].position};
            if (!IsOnSide(corners, headRadius)) {
                continue;
            }
            SideTriangle triangle{.isMain = code == kMainColour, .rgba = rgba};
            for (std::size_t k = 0; k < corners.size(); ++k) {
                triangle.corners.at(k) = {PhiOf(corners.at(k)),
                                          -static_cast<double>(corners.at(k).z)};
            }
            triangles.push_back(triangle);
        }
    }
    std::ranges::stable_sort(triangles, std::ranges::greater{}, &SideTriangle::isMain);
    return triangles;
}

void PlaceOnCanvas(std::vector<SideTriangle>& triangles) {
    double top = std::numeric_limits<double>::infinity();
    double bottom = -std::numeric_limits<double>::infinity();
    for (const SideTriangle& triangle : triangles) {
        for (const CanvasPoint& corner : triangle.corners) {
            top = std::min(top, corner[1]);
            bottom = std::max(bottom, corner[1]);
        }
    }
    for (SideTriangle& triangle : triangles) {
        for (CanvasPoint& corner : triangle.corners) {
            corner = {(corner[0] + kQuarterTurnDegrees) / kHalfTurnDegrees * kCanvasWidth,
                      (corner[1] - top) / (bottom - top) * kCanvasHeight};
        }
    }
}

std::array<double, kFaceDrawingSizePx> SampleColumns() {
    std::array<double, kFaceDrawingSizePx> columns{};
    for (std::size_t i = 0; i < columns.size(); ++i) {
        const double u = (static_cast<double>(i) + 0.5) / kFaceDrawingSizePx;
        const double x =
            std::floor((PhiOfU(u) + kQuarterTurnDegrees) / kHalfTurnDegrees * kCanvasWidth);
        columns.at(i) = std::clamp(x, 0.0, kCanvasWidth - 1.0);
    }
    return columns;
}

std::array<double, kFaceDrawingSizePx> SampleRows() {
    std::array<double, kFaceDrawingSizePx> rows{};
    for (std::size_t j = 0; j < rows.size(); ++j) {
        const double v = (static_cast<double>(j) + 0.5) / kFaceDrawingSizePx;
        const double t = (v - kRowTop) / (kRowBottom - kRowTop);
        rows.at(j) = std::floor(std::clamp(t, 0.0, kLastRowFraction) * kCanvasHeight);
    }
    return rows;
}

double EdgeSide(const CanvasPoint& a, const CanvasPoint& b, double x, double y) {
    return ((b[0] - a[0]) * (y - a[1])) - ((b[1] - a[1]) * (x - a[0]));
}

bool Contains(const SideTriangle& triangle, double x, double y) {
    const auto& [a, b, c] = triangle.corners;
    const double area = EdgeSide(a, b, c[0], c[1]);
    return area != 0.0 && EdgeSide(a, b, x, y) * area >= 0.0 &&
           EdgeSide(b, c, x, y) * area >= 0.0 && EdgeSide(c, a, x, y) * area >= 0.0;
}

void SetPixel(ImageAsset& image, std::size_t x, std::size_t y, const Rgba8& rgba) {
    const std::size_t at = ((y * image.widthPx) + x) * kBytesPerPixel;
    for (std::size_t channel = 0; channel < kBytesPerPixel; ++channel) {
        image.rgba[at + channel] = static_cast<std::byte>(rgba.at(channel));
    }
}

void Paint(ImageAsset& image, const SideTriangle& triangle,
           const std::array<double, kFaceDrawingSizePx>& columns,
           const std::array<double, kFaceDrawingSizePx>& rows) {
    const auto [left, right] = std::ranges::minmax(
        triangle.corners | std::views::transform([](const CanvasPoint& p) { return p[0]; }));
    const auto [top, bottom] = std::ranges::minmax(
        triangle.corners | std::views::transform([](const CanvasPoint& p) { return p[1]; }));
    for (std::size_t y = 0; y < rows.size(); ++y) {
        if (rows.at(y) < top || rows.at(y) > bottom) {
            continue;
        }
        for (std::size_t x = 0; x < columns.size(); ++x) {
            if (columns.at(x) >= left && columns.at(x) <= right &&
                Contains(triangle, columns.at(x), rows.at(y))) {
                SetPixel(image, x, y, triangle.rgba);
            }
        }
    }
}

}

ImageAsset DrawHeadPrint(const GeometryAsset& head) {
    ImageAsset image = PlainFace(kYellow);
    std::vector<SideTriangle> triangles = SideTrianglesOf(head);
    if (triangles.empty()) {
        return image;
    }
    PlaceOnCanvas(triangles);
    const std::array<double, kFaceDrawingSizePx> columns = SampleColumns();
    const std::array<double, kFaceDrawingSizePx> rows = SampleRows();
    for (const SideTriangle& triangle : triangles) {
        Paint(image, triangle, columns, rows);
    }
    return image;
}

}
