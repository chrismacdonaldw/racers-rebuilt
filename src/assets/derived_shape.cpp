#include "assets/derived_shape.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace rr::assets {
namespace {

constexpr float kPlate = 0.4F;
constexpr float kInset = 0.05F;
constexpr float kCornerSize = 0.175F;
constexpr float kStudReach = 0.15F;
constexpr float kStudRise = 0.1F;
constexpr float kBottomPlate = 0.3F;
constexpr float kRounding = 0.05F;
constexpr float kSlack = 0.02F;
constexpr std::uint8_t kOccupied = 0x80;
constexpr std::uint8_t kRests = 0x80;
constexpr std::uint8_t kSitsLevel = 0x40;
constexpr float kHighestTop = kHighestPlate * kPlate;

using Triangle = std::array<Vector3, 3>;
using Polygon = std::vector<Vector3>;

struct Square {
    float left = 0.0F;
    float top = 0.0F;
    float right = 0.0F;
    float bottom = 0.0F;
};

struct Heights {
    float low = std::numeric_limits<float>::max();
    float high = std::numeric_limits<float>::lowest();

    [[nodiscard]] bool IsEmpty() const { return low > high; }
};

Polygon Clip(const Polygon& polygon, float Vector3::* axis, float limit, bool keepsAbove) {
    Polygon kept;
    const auto isKept = [&](const Vector3& point) {
        return keepsAbove ? point.*axis >= limit : point.*axis <= limit;
    };
    for (std::size_t corner = 0; corner < polygon.size(); ++corner) {
        const Vector3& from = polygon[corner];
        const Vector3& to = polygon[(corner + 1) % polygon.size()];
        if (isKept(from)) {
            kept.push_back(from);
        }
        if (isKept(from) != isKept(to)) {
            const float along = (limit - from.*axis) / (to.*axis - from.*axis);
            kept.push_back(from + ((to - from) * along));
        }
    }
    return kept;
}

bool Misses(const Triangle& triangle, const Square& square) {
    const auto [left, right] = std::ranges::minmax({triangle[0].x, triangle[1].x, triangle[2].x});
    const auto [front, back] = std::ranges::minmax({triangle[0].y, triangle[1].y, triangle[2].y});
    return right < square.left || left > square.right || back < square.top || front > square.bottom;
}

Heights HeightsIn(std::span<const Triangle> triangles, const Square& square) {
    Heights heights;
    for (const Triangle& triangle : triangles) {
        if (Misses(triangle, square)) {
            continue;
        }
        Polygon polygon(triangle.begin(), triangle.end());
        polygon = Clip(polygon, &Vector3::x, square.left, true);
        polygon = Clip(polygon, &Vector3::x, square.right, false);
        polygon = Clip(polygon, &Vector3::y, square.top, true);
        polygon = Clip(polygon, &Vector3::y, square.bottom, false);
        for (const Vector3& point : polygon) {
            heights.low = std::min(heights.low, point.z);
            heights.high = std::max(heights.high, point.z);
        }
    }
    return heights;
}

Square Around(float x, float y, float reach) {
    return {.left = x - reach, .top = y - reach, .right = x + reach, .bottom = y + reach};
}

ShapeCell DeriveCell(std::span<const Triangle> triangles, float x, float y) {
    constexpr std::array kCorners{kInset, 1.0F - kInset - kCornerSize};
    bool isCovered = false;
    bool rests = true;
    float high = std::numeric_limits<float>::lowest();
    for (const float across : kCorners) {
        for (const float deep : kCorners) {
            const Heights corner = HeightsIn(triangles, {.left = x + across,
                                                         .top = y + deep,
                                                         .right = x + across + kCornerSize,
                                                         .bottom = y + deep + kCornerSize});
            isCovered = isCovered || !corner.IsEmpty();
            rests = rests && !corner.IsEmpty() && corner.low < kBottomPlate;
            high = std::max(high, corner.high);
        }
    }
    if (!isCovered) {
        return {};
    }
    const int top = static_cast<int>(std::ceil((high / kPlate) - kRounding));
    const Heights stud = HeightsIn(triangles, Around(x + 0.5F, y + 0.5F, kStudReach));
    const bool isOccupied =
        !stud.IsEmpty() && stud.high > (static_cast<float>(top) * kPlate) + kStudRise;
    const Heights whole = HeightsIn(triangles, Around(x + 0.5F, y + 0.5F, 0.5F - kInset));
    const int bottom = std::max(0, static_cast<int>(std::floor((whole.low / kPlate) + kRounding)));
    std::uint8_t flags = 0;
    if (bottom == 0) {
        flags = rests ? kRests : kSitsLevel;
    }
    return {.top = static_cast<std::uint8_t>((isOccupied ? kOccupied : 0U) | top),
            .bottom = static_cast<std::uint8_t>(flags | bottom)};
}

}

bool IsOnBuildGrid(const GeometryAsset& model) {
    const auto isInside = [](float value, float high) {
        return std::isfinite(value) && value >= -kSlack && value <= high + kSlack;
    };
    const bool hasVertices = std::ranges::all_of(
        model.indices, [&](std::uint32_t index) { return index < model.vertices.size(); });
    return hasVertices && std::ranges::all_of(model.vertices, [&](const GeometryVertex& vertex) {
               const Vector3& at = vertex.position;
               return isInside(at.x, static_cast<float>(kBuildGridSize.width)) &&
                      isInside(at.y, static_cast<float>(kBuildGridSize.height)) &&
                      isInside(at.z, kHighestTop);
           });
}

PieceShape DeriveShape(const GeometryAsset& model) {
    std::vector<Triangle> triangles;
    triangles.reserve(model.indices.size() / 3);
    Vector3 extent;
    for (std::size_t index = 0; index + 2 < model.indices.size(); index += 3) {
        Triangle& triangle = triangles.emplace_back();
        for (std::size_t corner = 0; corner < 3; ++corner) {
            triangle.at(corner) = model.vertices.at(model.indices[index + corner]).position;
            extent.x = std::max(extent.x, triangle.at(corner).x);
            extent.y = std::max(extent.y, triangle.at(corner).y);
        }
    }
    PieceShape shape{.size = {.width = static_cast<int>(std::ceil(extent.x - kSlack)),
                              .height = static_cast<int>(std::ceil(extent.y - kSlack))}};
    for (int x = 0; x < shape.size.width; ++x) {
        for (int y = 0; y < shape.size.height; ++y) {
            shape.cells.push_back(
                DeriveCell(triangles, static_cast<float>(x), static_cast<float>(y)));
        }
    }
    return shape;
}

}
