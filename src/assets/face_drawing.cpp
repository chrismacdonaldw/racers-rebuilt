#include "assets/face_drawing.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <utility>

#include "assets/face_shapes.hpp"
#include "assets/game_path.hpp"

namespace rr::assets {
namespace {

constexpr std::array<std::string_view, kFaceExpressionCount> kSections{"DFLT",  "ANGRY", "BLINK",
                                                                       "HAPPY", "SAD",   "SUPRZ"};
constexpr std::size_t kBytesPerPixel = 4;
constexpr std::size_t kHexDigits = 6;
constexpr int kHexBase = 16;
constexpr std::uint8_t kOpaque = 255;
constexpr std::size_t kEllipseNumbers = 4;
constexpr std::size_t kRotatedEllipseNumbers = 5;
constexpr std::size_t kPolygonCorners = 3;

using Tokens = std::span<const std::string_view>;

std::vector<std::string_view> Split(std::string_view line) {
    std::vector<std::string_view> tokens;
    constexpr std::string_view kSpace = " \t\r";
    std::size_t at = line.find_first_not_of(kSpace);
    while (at != std::string_view::npos) {
        const std::size_t end = std::min(line.find_first_of(kSpace, at), line.size());
        tokens.push_back(line.substr(at, end - at));
        at = line.find_first_not_of(kSpace, end);
    }
    return tokens;
}

std::unexpected<Error> Bad(std::string message) {
    return Fail(ErrorCode::InvalidData, std::move(message));
}

std::optional<double> NumberOf(std::string_view token) {
    double value = 0.0;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error != std::errc{} || end != token.data() + token.size() || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

Result<Rgba8> ColourOf(std::string_view token) {
    std::uint32_t value = 0;
    const auto [end, error] =
        std::from_chars(token.data(), token.data() + token.size(), value, kHexBase);
    if (token.size() != kHexDigits || error != std::errc{} || end != token.data() + token.size()) {
        return Bad(std::format("{} isn't an RRGGBB colour", token));
    }
    constexpr unsigned kByte = 0xFFU;
    constexpr unsigned kRedShift = 16;
    constexpr unsigned kGreenShift = 8;
    return Rgba8{static_cast<std::uint8_t>((value >> kRedShift) & kByte),
                 static_cast<std::uint8_t>((value >> kGreenShift) & kByte),
                 static_cast<std::uint8_t>(value & kByte), kOpaque};
}

Result<std::vector<double>> NumbersOf(Tokens tokens) {
    std::vector<double> numbers;
    for (const std::string_view token : tokens) {
        const std::optional<double> number = NumberOf(token);
        if (!number) {
            return Bad(std::format("{} isn't a number", token));
        }
        numbers.push_back(*number);
    }
    return numbers;
}

Result<std::vector<FacePoint>> PointsOf(std::span<const double> numbers, std::size_t minimum) {
    if (numbers.size() % 2 != 0 || numbers.size() < minimum * 2) {
        return Bad(std::format("needs at least {} x y pairs", minimum));
    }
    std::vector<FacePoint> points;
    for (std::size_t i = 0; i < numbers.size(); i += 2) {
        points.push_back({numbers[i], numbers[i + 1]});
    }
    return points;
}

Result<FaceShape> EllipseOf(std::span<const double> numbers) {
    if (numbers.size() != kEllipseNumbers && numbers.size() != kRotatedEllipseNumbers) {
        return Bad("an ellipse is cx cy rx ry and an optional angle");
    }
    if (numbers[2] <= 0.0 || numbers[3] <= 0.0) {
        return Bad("an ellipse needs positive radii");
    }
    return FaceEllipse{.centre = {numbers[0], numbers[1]},
                       .radii = {numbers[2], numbers[3]},
                       .degrees = numbers.size() == kRotatedEllipseNumbers ? numbers[4] : 0.0};
}

Result<FaceShape> ShapeOf(Tokens tokens) {
    if (tokens.empty()) {
        return Bad("missing shape");
    }
    const std::string_view kind = tokens.front();
    if (kind == "all" && tokens.size() == 1) {
        return FaceEverywhere{};
    }
    Result<std::vector<double>> numbers = NumbersOf(tokens.subspan(1));
    if (!numbers) {
        return std::unexpected(numbers.error());
    }
    if (kind == "poly") {
        return PointsOf(*numbers, kPolygonCorners).transform([](std::vector<FacePoint> points) {
            return FaceShape{FacePolygon{.points = std::move(points)}};
        });
    }
    if (kind == "line" && !numbers->empty() && numbers->front() > 0.0) {
        const double width = numbers->front();
        return PointsOf(std::span(*numbers).subspan(1), 1)
            .transform([&](std::vector<FacePoint> points) {
                return FaceShape{FaceLine{.width = width, .points = std::move(points)}};
            });
    }
    if (kind == "ellipse") {
        return EllipseOf(*numbers);
    }
    return Bad(std::format("bad {} shape", kind));
}

Result<FaceOp> OpOf(Tokens tokens) {
    const std::string_view op = tokens.front();
    if (op == "restore") {
        return ShapeOf(tokens.subspan(1)).transform([](FaceShape shape) {
            return FaceOp{FaceRestore{.shape = std::move(shape)}};
        });
    }
    if (op == "fill" && tokens.size() > 1) {
        return ColourOf(tokens[1]).and_then([&](Rgba8 colour) {
            return ShapeOf(tokens.subspan(2)).transform([&](FaceShape shape) {
                return FaceOp{FaceFill{.colour = colour, .shape = std::move(shape)}};
            });
        });
    }
    if (op == "recolour" && tokens.size() > 2) {
        Result<Rgba8> from = ColourOf(tokens[1]);
        Result<Rgba8> to = ColourOf(tokens[2]);
        if (!from || !to) {
            return std::unexpected(from ? to.error() : from.error());
        }
        return ShapeOf(tokens.subspan(3)).transform([&](FaceShape shape) {
            return FaceOp{FaceRecolour{.from = *from, .to = *to, .shape = std::move(shape)}};
        });
    }
    return Bad(std::format("unknown op {}", op));
}

struct ParseState {
    FaceDrawing drawing;
    bool hasBase = false;
    std::optional<std::size_t> section;
};

Result<void> ReadBase(ParseState& state, Tokens tokens) {
    if (state.hasBase || tokens.size() != 3) {
        return Bad("needs one base: ldraw <part> or plain RRGGBB");
    }
    state.hasBase = true;
    if (tokens[1] == "ldraw") {
        state.drawing.ldrawPart = std::string(tokens[2]);
        return {};
    }
    if (tokens[1] != "plain") {
        return Bad(std::format("unknown base {}", tokens[1]));
    }
    return ColourOf(tokens[2]).transform([&](Rgba8 colour) { state.drawing.plain = colour; });
}

Result<void> ReadSection(ParseState& state, std::string_view line) {
    const std::string name = ToUpperAscii(line.substr(1, line.size() - 2));
    const auto found = std::ranges::find(kSections, name);
    if (!line.ends_with(']') || found == kSections.end()) {
        return Bad(std::format("unknown section {}", line));
    }
    state.section = static_cast<std::size_t>(found - kSections.begin());
    return {};
}

Result<void> ReadStatement(ParseState& state, std::string_view line) {
    if (line.starts_with('[')) {
        return ReadSection(state, line);
    }
    const std::vector<std::string_view> tokens = Split(line);
    if (tokens.front() == "base") {
        return ReadBase(state, tokens);
    }
    if (!state.section) {
        return Bad("op before a section");
    }
    return OpOf(tokens).transform(
        [&](FaceOp op) { state.drawing.expressions.at(*state.section).push_back(std::move(op)); });
}

std::string_view Statement(std::string_view line) {
    line = line.substr(0, line.find('#'));
    const std::size_t first = line.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    return line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
}

Rgba8 PixelAt(const ImageAsset& image, std::size_t pixel) {
    const std::size_t at = pixel * kBytesPerPixel;
    return {std::to_integer<std::uint8_t>(image.rgba[at]),
            std::to_integer<std::uint8_t>(image.rgba[at + 1]),
            std::to_integer<std::uint8_t>(image.rgba[at + 2]),
            std::to_integer<std::uint8_t>(image.rgba[at + 3])};
}

void SetPixel(ImageAsset& image, std::size_t pixel, const Rgba8& rgba) {
    const std::size_t at = pixel * kBytesPerPixel;
    for (std::size_t channel = 0; channel < kBytesPerPixel; ++channel) {
        image.rgba[at + channel] = static_cast<std::byte>(rgba.at(channel));
    }
}

void Apply(ImageAsset& image, const ImageAsset& start, const FaceOp& op) {
    const auto& shape =
        std::visit([](const auto& kind) -> const FaceShape& { return kind.shape; }, op);
    const std::vector<std::uint8_t> mask = ShapeMask(shape);
    for (std::size_t pixel = 0; pixel < mask.size(); ++pixel) {
        if (mask[pixel] == 0) {
            continue;
        }
        if (const auto* fill = std::get_if<FaceFill>(&op)) {
            SetPixel(image, pixel, fill->colour);
        } else if (const auto* recolour = std::get_if<FaceRecolour>(&op)) {
            if (PixelAt(image, pixel) == recolour->from) {
                SetPixel(image, pixel, recolour->to);
            }
        } else {
            SetPixel(image, pixel, PixelAt(start, pixel));
        }
    }
}

ImageAsset Drawn(const ImageAsset& start, std::span<const FaceOp> ops) {
    ImageAsset image{.widthPx = start.widthPx, .heightPx = start.heightPx, .rgba = start.rgba};
    for (const FaceOp& op : ops) {
        Apply(image, start, op);
    }
    return image;
}

}

Result<FaceDrawing> ParseFaceDrawing(std::string_view text) {
    ParseState state;
    std::size_t lineNumber = 0;
    for (const auto line : std::views::split(text, '\n')) {
        ++lineNumber;
        const std::string_view statement = Statement(std::string_view(line));
        if (statement.empty()) {
            continue;
        }
        if (Result<void> read = ReadStatement(state, statement); !read) {
            return Bad(std::format("Face line {}: {}", lineNumber, read.error().message));
        }
    }
    if (!state.hasBase) {
        return Bad("Face drawing has no base");
    }
    return std::move(state.drawing);
}

std::array<ImageAsset, kFaceExpressionCount> DrawFaces(const FaceDrawing& drawing,
                                                       const ImageAsset& base) {
    std::array<ImageAsset, kFaceExpressionCount> faces;
    faces[0] = Drawn(base, drawing.expressions[0]);
    for (std::size_t expression = 1; expression < kFaceExpressionCount; ++expression) {
        faces.at(expression) = Drawn(faces[0], drawing.expressions.at(expression));
    }
    return faces;
}

ImageAsset PlainFace(const Rgba8& colour) {
    ImageAsset image{.widthPx = kFaceDrawingSizePx,
                     .heightPx = kFaceDrawingSizePx,
                     .rgba = std::vector<std::byte>(std::size_t{kFaceDrawingSizePx} *
                                                    kFaceDrawingSizePx * kBytesPerPixel)};
    for (std::size_t pixel = 0; pixel < std::size_t{kFaceDrawingSizePx} * kFaceDrawingSizePx;
         ++pixel) {
        SetPixel(image, pixel, colour);
    }
    return image;
}

}
