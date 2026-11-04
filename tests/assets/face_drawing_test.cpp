#include "assets/face_drawing.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace rr::assets {
namespace {

constexpr Rgba8 kYellow{0xFA, 0xC8, 0x0A, 0xFF};
constexpr Rgba8 kBlack{0x1B, 0x2A, 0x34, 0xFF};
constexpr Rgba8 kWhite{0xF4, 0xF4, 0xF4, 0xFF};
constexpr Rgba8 kRed{0xB4, 0x00, 0x00, 0xFF};

constexpr std::size_t kDefault = 0;
constexpr std::size_t kAngry = 1;
constexpr std::size_t kBlink = 2;
constexpr std::size_t kHappy = 3;

Rgba8 PixelAt(const ImageAsset& image, std::size_t x, std::size_t y) {
    const std::size_t at = ((y * image.widthPx) + x) * 4;
    return {std::to_integer<std::uint8_t>(image.rgba[at]),
            std::to_integer<std::uint8_t>(image.rgba[at + 1]),
            std::to_integer<std::uint8_t>(image.rgba[at + 2]),
            std::to_integer<std::uint8_t>(image.rgba[at + 3])};
}

std::array<ImageAsset, kFaceExpressionCount> Drawn(std::string_view text) {
    const Result<FaceDrawing> drawing = ParseFaceDrawing(text);
    REQUIRE(drawing.has_value());
    return DrawFaces(*drawing, PlainFace(drawing->plain));
}

std::string ErrorOf(std::string_view text) {
    const Result<FaceDrawing> drawing = ParseFaceDrawing(text);
    return drawing ? std::string{} : drawing.error().message;
}

TEST_CASE("a face drawing reads its base, sections and ops") {
    const Result<FaceDrawing> drawing = ParseFaceDrawing(
        "# comment\n\nbase ldraw 3626bp3k.dat\n[dflt]\nfill 1B2A34 ellipse 10 10 4 4 30\n"
        "[BLINK]  # eyes shut\nrecolour 1B2A34 F4F4F4 all\nrestore line 3 1 1 5 5\n"
        "[SUPRZ]\nfill b40000 poly 0 0 10 0 10 10\n");

    REQUIRE(drawing.has_value());
    CHECK(drawing->ldrawPart == "3626bp3k.dat");
    REQUIRE(drawing->expressions[kDefault].size() == 1);
    const auto* fill = std::get_if<FaceFill>(drawing->expressions[kDefault].data());
    REQUIRE(fill != nullptr);
    CHECK(fill->colour == kBlack);
    CHECK(std::get<FaceEllipse>(fill->shape).degrees == 30.0);
    CHECK(drawing->expressions[kBlink].size() == 2);
    CHECK(drawing->expressions[kAngry].empty());
    CHECK(std::get<FaceFill>(drawing->expressions[5][0]).colour == kRed);
}

TEST_CASE("a plain base reads its colour") {
    const Result<FaceDrawing> drawing = ParseFaceDrawing("base plain FAC80A\n");

    REQUIRE(drawing.has_value());
    CHECK(drawing->ldrawPart.empty());
    CHECK(drawing->plain == kYellow);
}

TEST_CASE("face drawing errors name the line") {
    CHECK(ErrorOf("base plain FAC80A\nfill 1B2A34 all\n").starts_with("Face line 2:"));
    CHECK(ErrorOf("base plain FAC80A\n[GRUMPY]\n").starts_with("Face line 2:"));
    CHECK(ErrorOf("base plain FAC80A\n[DFLT]\npaint 1B2A34 all\n").starts_with("Face line 3:"));
    CHECK(ErrorOf("base plain FAC80\n").starts_with("Face line 1:"));
    CHECK(ErrorOf("base plain FAC80A\nbase plain FAC80A\n").starts_with("Face line 2:"));
}

std::string OpError(std::string_view op) {
    return ErrorOf(std::string("base plain FAC80A\n[DFLT]\n") + std::string(op));
}

TEST_CASE("face drawing shapes need their numbers") {
    constexpr std::array kBadOps{
        "fill 1B2A34 poly 0 0 10 10", "fill 1B2A34 poly 0 0 10 10 5",  "fill 1B2A34 line 3",
        "fill 1B2A34 line 0 1 1",     "fill 1B2A34 ellipse 1 1 0 4",   "fill 1B2A34 ellipse 1 1 4",
        "fill 1B2A34 all 4",          "fill 1B2A34 poly 0 0 10 x 5 5", "fill 1B2A34"};

    for (const std::string_view op : kBadOps) {
        CHECK_MESSAGE(!OpError(op).empty(), op);
    }
    CHECK(OpError("fill 1B2A34 line 3 1 1").empty());
}

TEST_CASE("a face drawing needs a base") {
    CHECK(ErrorOf("[DFLT]\nfill 1B2A34 all\n") == "Face drawing has no base");
}

TEST_CASE("default ops show in every expression, and expression ops only in their own") {
    const auto faces = Drawn(
        "base plain FAC80A\n[DFLT]\nfill 1B2A34 poly 0 0 50 0 50 50 0 50\n"
        "[HAPPY]\nfill B40000 poly 100 100 150 100 150 150 100 150\n");

    CHECK(std::ranges::all_of(faces, [](const ImageAsset& face) {
        return face.widthPx == kFaceDrawingSizePx && PixelAt(face, 25, 25) == kBlack;
    }));
    CHECK(PixelAt(faces[kHappy], 125, 125) == kRed);
    CHECK(PixelAt(faces[kDefault], 125, 125) == kYellow);
    CHECK(PixelAt(faces[kAngry], 125, 125) == kYellow);
}

TEST_CASE("recolour changes only pixels of its colour") {
    const auto faces = Drawn(
        "base plain FAC80A\n[DFLT]\nfill 1B2A34 poly 0 0 50 0 50 50 0 50\n"
        "[ANGRY]\nrecolour 1B2A34 F4F4F4 all\n");

    CHECK(PixelAt(faces[kAngry], 25, 25) == kWhite);
    CHECK(PixelAt(faces[kAngry], 125, 125) == kYellow);
}

TEST_CASE("restore goes back to the base in the default face, and to the default face in others") {
    const auto faces = Drawn(
        "base plain FAC80A\n[DFLT]\nfill 1B2A34 all\nrestore poly 0 0 50 0 50 50 0 50\n"
        "[BLINK]\nfill F4F4F4 all\nrestore poly 100 100 150 100 150 150 100 150\n");

    CHECK(PixelAt(faces[kDefault], 25, 25) == kYellow);
    CHECK(PixelAt(faces[kDefault], 125, 125) == kBlack);
    CHECK(PixelAt(faces[kBlink], 125, 125) == kBlack);
    CHECK(PixelAt(faces[kBlink], 25, 200) == kWhite);
}

}
}
