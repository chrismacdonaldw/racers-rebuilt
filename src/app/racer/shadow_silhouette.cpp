#include "app/racer/shadow_silhouette.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <vector>

namespace rr::app {
namespace {

constexpr auto kSize = static_cast<float>(kSilhouettePx);
constexpr std::size_t kChannels = 4;
constexpr std::array kShade{std::byte{8}, std::byte{8}, std::byte{8}, std::byte{255}};
constexpr float kHalf = 0.5F;

struct Pixel {
    float column = 0.0F;
    float row = 0.0F;
};

using Triangle = std::array<Pixel, 3>;

float Edge(const Pixel& from, const Pixel& to, const Pixel& point) {
    return ((to.column - from.column) * (point.row - from.row)) -
           ((to.row - from.row) * (point.column - from.column));
}

bool IsInside(const Pixel& pixel) {
    return pixel.column >= 0.0F && pixel.column <= kSize && pixel.row >= 0.0F && pixel.row <= kSize;
}

std::uint32_t Lowest(float a, float b, float c) {
    return static_cast<std::uint32_t>(std::floor(std::min({a, b, c})));
}

std::uint32_t Highest(float a, float b, float c) {
    return std::min(kSilhouettePx, static_cast<std::uint32_t>(std::ceil(std::max({a, b, c}))));
}

bool Covers(const Triangle& triangle, float area, const Pixel& centre) {
    const float sign = area > 0.0F ? 1.0F : -1.0F;
    return Edge(triangle[1], triangle[2], centre) * sign >= 0.0F &&
           Edge(triangle[2], triangle[0], centre) * sign >= 0.0F &&
           Edge(triangle[0], triangle[1], centre) * sign >= 0.0F;
}

void Fill(const Triangle& triangle, std::vector<std::byte>& rgba) {
    const float area = Edge(triangle[0], triangle[1], triangle[2]);
    if (area == 0.0F) {
        return;
    }
    const auto& [a, b, c] = triangle;
    for (std::uint32_t row = Lowest(a.row, b.row, c.row); row < Highest(a.row, b.row, c.row);
         ++row) {
        for (std::uint32_t column = Lowest(a.column, b.column, c.column);
             column < Highest(a.column, b.column, c.column); ++column) {
            const Pixel centre{.column = static_cast<float>(column) + kHalf,
                               .row = static_cast<float>(row) + kHalf};
            if (Covers(triangle, area, centre)) {
                const std::size_t pixel = (std::size_t{row} * kSilhouettePx) + column;
                std::ranges::copy(
                    kShade,
                    std::next(rgba.begin(), static_cast<std::ptrdiff_t>(pixel * kChannels)));
            }
        }
    }
}

}

assets::ImageAsset ShadowSilhouette(std::span<const Vector3> corners, float length, float width) {
    assets::ImageAsset image{
        .widthPx = kSilhouettePx,
        .heightPx = kSilhouettePx,
        .rgba = std::vector<std::byte>(std::size_t{kSilhouettePx} * kSilhouettePx * kChannels)};
    const auto toPixel = [&](const Vector3& corner) {
        return Pixel{.column = (corner.y + (width * kHalf)) * kSize / width,
                     .row = (corner.x + (length * kHalf)) * kSize / length};
    };
    for (std::size_t i = 0; i + 2 < corners.size(); i += 3) {
        const Triangle triangle{toPixel(corners[i]), toPixel(corners[i + 1]),
                                toPixel(corners[i + 2])};
        if (std::ranges::all_of(triangle, IsInside)) {
            Fill(triangle, image.rgba);
        }
    }
    return image;
}

}
