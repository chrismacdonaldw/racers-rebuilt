#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "assets/image_asset.hpp"
#include "core/color.hpp"
#include "core/result.hpp"

namespace rr::assets {

inline constexpr std::uint32_t kFaceDrawingSizePx = 256;
inline constexpr std::size_t kFaceExpressionCount = 6;

using FacePoint = std::array<double, 2>;

struct FacePolygon {
    std::vector<FacePoint> points;
};

struct FaceLine {
    double width = 0.0;
    std::vector<FacePoint> points;
};

struct FaceEllipse {
    FacePoint centre{};
    FacePoint radii{};
    double degrees = 0.0;
};

struct FaceEverywhere {};

using FaceShape = std::variant<FacePolygon, FaceLine, FaceEllipse, FaceEverywhere>;

struct FaceFill {
    Rgba8 colour{};
    FaceShape shape;
};

struct FaceRecolour {
    Rgba8 from{};
    Rgba8 to{};
    FaceShape shape;
};

struct FaceRestore {
    FaceShape shape;
};

using FaceOp = std::variant<FaceFill, FaceRecolour, FaceRestore>;

/// An empty `ldrawPart` means the base is `plain` all over.
struct FaceDrawing {
    std::string ldrawPart;
    Rgba8 plain{};
    std::array<std::vector<FaceOp>, kFaceExpressionCount> expressions;
};

/// Errors name the 1-based line.
[[nodiscard]] Result<FaceDrawing> ParseFaceDrawing(std::string_view text);

[[nodiscard]] ImageAsset PlainFace(const Rgba8& colour);

/// Expressions in `FaceMaterial` order. `base` is `kFaceDrawingSizePx` square.
[[nodiscard]] std::array<ImageAsset, kFaceExpressionCount> DrawFaces(const FaceDrawing& drawing,
                                                                     const ImageAsset& base);

}
