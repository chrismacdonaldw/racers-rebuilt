#include "assets/ldraw_pieces.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "archive_survey.hpp"
#include "assets/ldraw_part.hpp"
#include "assets/piece_library.hpp"
#include "sim/car_build/mesh_primitives.hpp"
#include "temp_folder.hpp"

namespace rr::assets {
namespace {

using Triangle = std::array<Vector3, 3>;

constexpr float kGameUnitsPerPlate = 0.4F;
constexpr float kTolerance = 0.0001F;
constexpr float kHeightTolerance = 0.05F;
constexpr std::array kSampleOffsets{0.1F, 0.9F};

std::optional<float> TopAt(const std::vector<Triangle>& triangles, float x, float y) {
    std::optional<float> top;
    for (const Triangle& t : triangles) {
        const float d =
            ((t[1].y - t[2].y) * (t[0].x - t[2].x)) + ((t[2].x - t[1].x) * (t[0].y - t[2].y));
        if (std::abs(d) < 1e-6F) {
            continue;
        }
        const float a =
            (((t[1].y - t[2].y) * (x - t[2].x)) + ((t[2].x - t[1].x) * (y - t[2].y))) / d;
        const float b =
            (((t[2].y - t[0].y) * (x - t[2].x)) + ((t[0].x - t[2].x) * (y - t[2].y))) / d;
        const float c = 1.0F - a - b;
        if (a < -1e-4F || b < -1e-4F || c < -1e-4F) {
            continue;
        }
        const float z = (a * t[0].z) + (b * t[1].z) + (c * t[2].z);
        top = top ? std::max(*top, z) : z;
    }
    return top;
}

std::vector<Triangle> OriginalTriangles(const PieceLibrary& library, const PieceRecord& record) {
    sim::MeshPrimitives primitives;
    REQUIRE(primitives.AddPiece(library, {.piece = &record}, false).has_value());
    const auto at = [&](std::uint32_t index) {
        Vector3 position = primitives.Vertex(index).position;
        position.z *= kGameUnitsPerPlate;
        return position;
    };
    std::vector<Triangle> triangles;
    for (std::size_t slot = 0; slot < primitives.Live(); ++slot) {
        const sim::MeshPrimitive& primitive = primitives.At(slot);
        triangles.push_back(
            {at(primitive.corners[0]), at(primitive.corners[1]), at(primitive.corners[2])});
        if (primitive.cornerCount == 4) {
            triangles.push_back(
                {at(primitive.corners[3]), at(primitive.corners[1]), at(primitive.corners[2])});
        }
    }
    return triangles;
}

std::vector<Triangle> PlacedTriangles(const GeometryAsset& part, const LdrawMatrix& matrix) {
    const auto at = [&](std::uint32_t index) {
        return Apply(matrix, part.vertices[index].position);
    };
    std::vector<Triangle> triangles;
    for (std::size_t i = 0; i + 2 < part.indices.size(); i += 3) {
        triangles.push_back(
            {at(part.indices[i]), at(part.indices[i + 1]), at(part.indices[i + 2])});
    }
    return triangles;
}

Vector3 Highest(const GeometryAsset& part, const LdrawMatrix& matrix) {
    Vector3 high = Apply(matrix, part.vertices.front().position);
    for (const GeometryVertex& vertex : part.vertices) {
        const Vector3 p = Apply(matrix, vertex.position);
        high = {.x = std::max(high.x, p.x), .y = std::max(high.y, p.y), .z = std::max(high.z, p.z)};
    }
    return high;
}

Vector3 Lowest(const GeometryAsset& part, const LdrawMatrix& matrix) {
    Vector3 low = Apply(matrix, part.vertices.front().position);
    for (const GeometryVertex& vertex : part.vertices) {
        const Vector3 p = Apply(matrix, vertex.position);
        low = {.x = std::min(low.x, p.x), .y = std::min(low.y, p.y), .z = std::min(low.z, p.z)};
    }
    return low;
}

std::vector<std::string_view> MappedPieces() {
    std::vector<std::string_view> pieces;
    for (const LdrawPart& part : LdrawParts()) {
        if (pieces.empty() || pieces.back() != part.piece) {
            pieces.push_back(part.piece);
        }
    }
    return pieces;
}

bool StaysOnCells(LdrawLibrary& library, std::string_view piece) {
    const Result<GeometryAsset> model = LoadLdrawPiece(library, LdrawPartsOf(piece));
    if (!model) {
        return false;
    }
    const Vector3 low = Lowest(*model, {});
    return low.x > -kTolerance && low.y > -kTolerance && low.z > -kTolerance;
}

std::vector<std::array<float, 2>> SamplePoints(const PieceRecord& record) {
    std::vector<std::array<float, 2>> points;
    points.reserve(static_cast<std::size_t>(record.shape.size.width * record.shape.size.height) *
                   kSampleOffsets.size() * kSampleOffsets.size());
    for (int cx = 0; cx < record.shape.size.width; ++cx) {
        for (int cy = 0; cy < record.shape.size.height; ++cy) {
            for (const float fx : kSampleOffsets) {
                for (const float fy : kSampleOffsets) {
                    points.push_back({static_cast<float>(cx) + fx, static_cast<float>(cy) + fy});
                }
            }
        }
    }
    return points;
}

bool FitsCells(const PieceLibrary& original, LdrawLibrary& library, std::string_view piece) {
    const PieceRecord* record = original.Find(piece);
    const Result<GeometryAsset> model = LoadLdrawPiece(library, LdrawPartsOf(piece));
    if (record == nullptr || !model) {
        return false;
    }
    const Vector3 high = Highest(*model, {});
    return high.x < static_cast<float>(record->shape.size.width) + kTolerance &&
           high.y < static_cast<float>(record->shape.size.height) + kTolerance;
}

std::optional<int> TopMismatches(const PieceLibrary& original, LdrawLibrary& library,
                                 const LdrawPart& mapped) {
    const PieceRecord* record = original.Find(mapped.piece);
    const Result<GeometryAsset> part = library.LoadPart(mapped.file);
    if (record == nullptr || !part) {
        return std::nullopt;
    }
    const std::vector<Triangle> expected = OriginalTriangles(original, *record);
    const std::vector<Triangle> placed = PlacedTriangles(*part, mapped.matrix);
    int mismatches = 0;
    for (const auto& [x, y] : SamplePoints(*record)) {
        const std::optional<float> want = TopAt(expected, x, y);
        const std::optional<float> got = TopAt(placed, x, y);
        if (want.has_value() != got.has_value() ||
            (want && got && std::abs(*want - *got) >= kHeightTolerance)) {
            ++mismatches;
        }
    }
    return mismatches;
}

std::string Misfits(const PieceLibrary& original, LdrawLibrary& library) {
    std::string misfits;
    for (const std::string_view piece : MappedPieces()) {
        const std::span<const LdrawPart> parts = LdrawPartsOf(piece);
        const bool topsMatch =
            parts.size() != 1 || TopMismatches(original, library, parts.front()) == 0;
        if (!FitsCells(original, library, piece) || !topsMatch) {
            misfits.append(piece).append(" ");
        }
    }
    return misfits;
}

TEST_CASE("every mapped piece loads from the shipped parts and stays above its first cell") {
    LdrawLibrary library(RR_LDRAW_DIR);

    for (const std::string_view piece : MappedPieces()) {
        CAPTURE(piece);
        CHECK(StaysOnCells(library, piece));
    }
}

TEST_CASE("a piece's parts are found together by name in any case") {
    std::vector<std::string_view> pieces = MappedPieces();
    std::ranges::sort(pieces);
    CHECK(std::ranges::adjacent_find(pieces) == pieces.end());
    REQUIRE(LdrawPartsOf("l300100").size() == 1);
    CHECK(LdrawPartsOf("l300100").front().file == "3001.dat");
    CHECK(LdrawPartsOf("SPRTSWD").size() == 2);
    CHECK(LdrawPartsOf("L999900").empty());
}

TEST_CASE("a piece model names LDraw colours as game materials, the main colour as the piece's") {
    const test::TempFolder folder;
    std::filesystem::create_directories(folder.Path() / "parts");
    std::ofstream(folder.Path() / "parts/x.dat")
        << "3 4 0 0 0 20 0 0 0 0 20\n3 16 0 0 0 20 0 0 0 0 20\n3 999 0 0 0 20 0 0 0 0 20\n";
    LdrawLibrary library(folder.Path());

    const std::array<LdrawPart, 2> parts{
        LdrawPart{.piece = "X", .file = "x.dat", .matrix = {.position = {.x = 1.0F}}},
        LdrawPart{.piece = "X", .file = "x.dat", .colour = "0"}};

    const Result<GeometryAsset> model = LoadLdrawPiece(library, parts);

    REQUIRE(model.has_value());
    CHECK(model->materials == std::vector<std::string>{"red", "piece", "black"});
    const std::size_t single = library.LoadPart("x.dat").value_or(GeometryAsset{}).vertices.size();
    REQUIRE(single > 0);
    REQUIRE(model->vertices.size() == 2 * single);
    CHECK(model->vertices[0].position == Vector3{.x = 1.0F});
    CHECK(model->vertices[single].position == Vector3{});
    CHECK(model->indices.back() >= single);
}

TEST_CASE(
    "mapped pieces fit their cells, and single parts have the original tops away from studs") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const Result<PieceLibrary> original =
        vfs->Read("MENUDATA/PIECEDB/LPIECEHI.LEB").and_then([](const FileData& data) {
            return ParsePieceLibrary(data.Bytes());
        });
    REQUIRE(original.has_value());
    LdrawLibrary library(RR_LDRAW_DIR);

    const std::string misfits = Misfits(*original, library);
    CHECK_MESSAGE(misfits.empty(), misfits);
}

}
}
