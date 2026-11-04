#include "assets/ldraw_prints.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

#include "core/vector3.hpp"

namespace rr::assets {
namespace {

constexpr std::size_t kBytesPerPixel = 4;
constexpr float kTorsoHalfWidth = 1.0F;
constexpr float kTorsoHeight = 1.6F;
constexpr float kFrontFacing = -0.9F;
constexpr std::uint32_t kFrontOversample = 2;
constexpr std::uint8_t kOpaque = 255;

struct PanelRow {
    float v = 0.0F;
    float left = 0.0F;
    float right = 0.0F;
};

constexpr PanelRow kShoulders{.v = 0.023F, .left = 0.199F, .right = 0.89F};
constexpr PanelRow kWaist{.v = 0.977F, .left = 0.095F, .right = 1.0F};
constexpr float kSideSwatchU = 0.039F;
constexpr float kSideSwatchV = 0.526F;

struct KnownColour {
    std::string_view code;
    Rgba8 rgba;
};

constexpr std::array kColours{
    KnownColour{.code = "0", .rgba = {0x1B, 0x2A, 0x34, kOpaque}},
    KnownColour{.code = "1", .rgba = {0x1E, 0x5A, 0xA8, kOpaque}},
    KnownColour{.code = "2", .rgba = {0x00, 0x85, 0x2B, kOpaque}},
    KnownColour{.code = "4", .rgba = {0xB4, 0x00, 0x00, kOpaque}},
    KnownColour{.code = "6", .rgba = {0x54, 0x33, 0x24, kOpaque}},
    KnownColour{.code = "7", .rgba = {0x8A, 0x92, 0x8D, kOpaque}},
    KnownColour{.code = "8", .rgba = {0x54, 0x59, 0x55, kOpaque}},
    KnownColour{.code = "14", .rgba = {0xFA, 0xC8, 0x0A, kOpaque}},
    KnownColour{.code = "15", .rgba = {0xF4, 0xF4, 0xF4, kOpaque}},
    KnownColour{.code = "71", .rgba = {0x96, 0x96, 0x96, kOpaque}},
    KnownColour{.code = "80", .rgba = {0x76, 0x76, 0x76, kOpaque}},
    KnownColour{.code = "82", .rgba = {0xDB, 0xAC, 0x34, kOpaque}},
    KnownColour{.code = "320", .rgba = {0x72, 0x00, 0x12, kOpaque}},
};

constexpr std::array kPrints{
    LdrawPrint{.torso = "GM_CHST", .file = "973p58.dat"},
    LdrawPrint{.torso = "IL_CHST", .file = "973p3j.dat"},
    LdrawPrint{.torso = "IN_CHST", .file = "973pz4.dat"},
    LdrawPrint{.torso = "KK_CHST", .file = "973p3h.dat"},
    LdrawPrint{.torso = "MM_CHST", .file = "973p81.dat"},
    LdrawPrint{.torso = "NH_CHST", .file = "973p55.dat"},
    LdrawPrint{.torso = "RK_CHST", .file = "973p4l.dat"},
};

bool SameName(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::toupper(static_cast<unsigned char>(x)) ==
               std::toupper(static_cast<unsigned char>(y));
    });
}

struct FlatTriangle {
    float depth = 0.0F;
    Rgba8 rgba{};
    std::array<std::array<float, 2>, 3> corners{};
};

class Canvas {
public:
    Canvas(std::uint32_t widthPx, std::uint32_t heightPx)
        : width_(widthPx), height_(heightPx), pixels_(std::size_t{widthPx} * heightPx) {}

    void Fill(const FlatTriangle& triangle);

    [[nodiscard]] std::uint32_t Height() const { return height_; }
    [[nodiscard]] std::optional<Rgba8> At(std::uint32_t x, std::uint32_t y) const {
        return pixels_[(std::size_t{y} * width_) + x];
    }
    [[nodiscard]] std::optional<std::array<std::uint32_t, 2>> CoveredColumns(
        std::uint32_t row) const;

private:
    std::uint32_t width_;
    std::uint32_t height_;
    std::vector<std::optional<Rgba8>> pixels_;
};

float EdgeSide(const std::array<float, 2>& a, const std::array<float, 2>& b, float x, float y) {
    return ((b[0] - a[0]) * (y - a[1])) - ((b[1] - a[1]) * (x - a[0]));
}

void Canvas::Fill(const FlatTriangle& triangle) {
    const auto& [a, b, c] = triangle.corners;
    const float area = EdgeSide(a, b, c[0], c[1]);
    if (area == 0.0F) {
        return;
    }
    const auto low = [](float p, float q, float r) {
        return static_cast<std::int64_t>(std::floor(std::min({p, q, r})));
    };
    const auto high = [](float p, float q, float r) {
        return static_cast<std::int64_t>(std::ceil(std::max({p, q, r})));
    };
    const std::int64_t x0 = std::max<std::int64_t>(0, low(a[0], b[0], c[0]));
    const std::int64_t x1 = std::min<std::int64_t>(width_, high(a[0], b[0], c[0]));
    const std::int64_t y0 = std::max<std::int64_t>(0, low(a[1], b[1], c[1]));
    const std::int64_t y1 = std::min<std::int64_t>(height_, high(a[1], b[1], c[1]));
    for (std::int64_t y = y0; y < y1; ++y) {
        for (std::int64_t x = x0; x < x1; ++x) {
            const float px = static_cast<float>(x) + 0.5F;
            const float py = static_cast<float>(y) + 0.5F;
            const bool isInside = EdgeSide(a, b, px, py) * area >= 0.0F &&
                                  EdgeSide(b, c, px, py) * area >= 0.0F &&
                                  EdgeSide(c, a, px, py) * area >= 0.0F;
            if (isInside) {
                pixels_[(static_cast<std::size_t>(y) * width_) + static_cast<std::size_t>(x)] =
                    triangle.rgba;
            }
        }
    }
}

std::optional<std::array<std::uint32_t, 2>> Canvas::CoveredColumns(std::uint32_t row) const {
    const auto line = std::span(pixels_).subspan(std::size_t{row} * width_, width_);
    const auto isCovered = [](const std::optional<Rgba8>& pixel) { return pixel.has_value(); };
    const auto first = std::ranges::find_if(line, isCovered);
    if (first == line.end()) {
        return std::nullopt;
    }
    const auto last = std::ranges::find_if(std::views::reverse(line), isCovered).base();
    return std::array{static_cast<std::uint32_t>(first - line.begin()),
                      static_cast<std::uint32_t>(last - line.begin())};
}

Rgba8 PixelAt(const ImageAsset& image, std::uint32_t x, std::uint32_t y) {
    const std::size_t at = ((std::size_t{y} * image.widthPx) + x) * kBytesPerPixel;
    return {std::to_integer<std::uint8_t>(image.rgba[at]),
            std::to_integer<std::uint8_t>(image.rgba[at + 1]),
            std::to_integer<std::uint8_t>(image.rgba[at + 2]),
            std::to_integer<std::uint8_t>(image.rgba[at + 3])};
}

void SetPixel(ImageAsset& image, std::uint32_t x, std::uint32_t y, const Rgba8& rgba) {
    const std::size_t at = ((std::size_t{y} * image.widthPx) + x) * kBytesPerPixel;
    for (std::size_t channel = 0; channel < kBytesPerPixel; ++channel) {
        image.rgba[at + channel] = static_cast<std::byte>(rgba.at(channel));
    }
}

std::uint32_t Scaled(float fraction, std::uint32_t sizePx) {
    return std::min(sizePx - 1, static_cast<std::uint32_t>(fraction * static_cast<float>(sizePx)));
}

std::vector<FlatTriangle> FrontTriangles(const GeometryAsset& part, const Rgba8& mainColour,
                                         std::uint32_t widthPx, std::uint32_t heightPx) {
    std::vector<FlatTriangle> triangles;
    for (const GeometryRange& range : part.ranges) {
        const Rgba8 rgba = LdrawColourOf(part.materials[range.material]).value_or(mainColour);
        for (std::uint32_t i = range.firstIndex; i + 2 < range.firstIndex + range.indexCount;
             i += 3) {
            const std::array corners{part.vertices[part.indices[i]].position,
                                     part.vertices[part.indices[i + 1]].position,
                                     part.vertices[part.indices[i + 2]].position};
            const Vector3 normal = Cross(corners[1] - corners[0], corners[2] - corners[0]);
            if (normal.y >= kFrontFacing * Length(normal)) {
                continue;
            }
            FlatTriangle flat{.depth = (corners[0].y + corners[1].y + corners[2].y) / 3.0F,
                              .rgba = rgba};
            for (std::size_t k = 0; k < corners.size(); ++k) {
                flat.corners.at(k) = {
                    (corners.at(k).x + kTorsoHalfWidth) / (2.0F * kTorsoHalfWidth) *
                        static_cast<float>(widthPx),
                    -corners.at(k).z / kTorsoHeight * static_cast<float>(heightPx)};
            }
            triangles.push_back(flat);
        }
    }
    std::ranges::sort(triangles, std::ranges::greater{}, &FlatTriangle::depth);
    return triangles;
}

ImageAsset Upscaled(const ImageAsset& image, std::uint32_t sizePx) {
    ImageAsset out{.widthPx = sizePx,
                   .heightPx = sizePx,
                   .rgba = std::vector<std::byte>(std::size_t{sizePx} * sizePx * kBytesPerPixel)};
    for (std::uint32_t y = 0; y < sizePx; ++y) {
        for (std::uint32_t x = 0; x < sizePx; ++x) {
            SetPixel(out, x, y,
                     PixelAt(image, x * image.widthPx / sizePx, y * image.heightPx / sizePx));
        }
    }
    return out;
}

void DrawPanelRow(ImageAsset& out, const Canvas& front, std::uint32_t y, const Rgba8& mainColour) {
    const float v = (static_cast<float>(y) + 0.5F) / static_cast<float>(out.heightPx);
    const float t = (v - kShoulders.v) / (kWaist.v - kShoulders.v);
    if (t < 0.0F || t > 1.0F) {
        return;
    }
    const auto row = Scaled(t, front.Height());
    const std::optional<std::array<std::uint32_t, 2>> span = front.CoveredColumns(row);
    if (!span) {
        return;
    }
    const float left = std::lerp(kShoulders.left, kWaist.left, t);
    const float right = std::lerp(kShoulders.right, kWaist.right, t);
    const auto size = static_cast<float>(out.widthPx);
    const auto first = static_cast<std::uint32_t>(std::ceil(left * size));
    const auto last = std::min(out.widthPx, static_cast<std::uint32_t>(right * size));
    for (std::uint32_t x = first; x < last; ++x) {
        const float s = (((static_cast<float>(x) + 0.5F) / size) - left) / (right - left);
        const auto column = (*span)[0] + static_cast<std::uint32_t>(
                                             s * static_cast<float>((*span)[1] - (*span)[0] - 1));
        SetPixel(out, x, y, front.At(column, row).value_or(mainColour));
    }
}

}

std::span<const LdrawPrint> LdrawPrints() {
    return kPrints;
}

std::optional<LdrawPrint> LdrawPrintFor(std::string_view torsoName) {
    const auto found = std::ranges::find_if(
        kPrints, [&](const LdrawPrint& print) { return SameName(print.torso, torsoName); });
    return found == kPrints.end() ? std::nullopt : std::optional(*found);
}

std::optional<Rgba8> LdrawColourOf(std::string_view code) {
    const auto found = std::ranges::find(kColours, code, &KnownColour::code);
    return found == kColours.end() ? std::nullopt : std::optional(found->rgba);
}

ImageAsset DrawTorsoPrint(const ImageAsset& torso, const GeometryAsset& part,
                          std::uint32_t sizePx) {
    if (torso.widthPx == 0 || torso.heightPx == 0 || sizePx == 0) {
        return torso;
    }
    const Rgba8 mainColour =
        PixelAt(torso, Scaled(kSideSwatchU, torso.widthPx), Scaled(kSideSwatchV, torso.heightPx));
    const std::uint32_t frontWidth = sizePx * kFrontOversample;
    const auto frontHeight = static_cast<std::uint32_t>(static_cast<float>(frontWidth) *
                                                        kTorsoHeight / (2.0F * kTorsoHalfWidth));
    Canvas front(frontWidth, frontHeight);
    for (const FlatTriangle& triangle : FrontTriangles(part, mainColour, frontWidth, frontHeight)) {
        front.Fill(triangle);
    }
    ImageAsset out = Upscaled(torso, sizePx);
    for (std::uint32_t y = 0; y < sizePx; ++y) {
        DrawPanelRow(out, front, y, mainColour);
    }
    return out;
}

}
