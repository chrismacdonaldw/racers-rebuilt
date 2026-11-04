#include "assets/minifig_fit.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archive_survey.hpp"
#include "assets/game_path.hpp"
#include "assets/geometry_database.hpp"
#include "assets/ldraw_part.hpp"
#include "assets/part_library_writer.hpp"
#include "assets/skeleton_database.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kMenuFolder = "MENUDATA/PARTDB/MENUPART";
constexpr std::string_view kRaceFolder = "MENUDATA/PARTDB/GAMEPART";

template <typename T>
T ReadRetail(const Vfs& vfs, const std::string& path,
             Result<T> (*parse)(std::span<const std::byte>)) {
    const Result<T> parsed =
        vfs.Read(path).and_then([parse](const FileData& data) { return parse(data.Bytes()); });
    REQUIRE(parsed.has_value());
    return parsed.value_or(T{});
}

struct Fitted {
    GeometryAsset original;
    SkeletonAsset skeleton;
    GeometryAsset model;
    FigureRig rig;
};

std::optional<Fitted> FitRetail(const Vfs& vfs, std::string_view folder, std::string_view body) {
    const std::string stem = std::format("{}/{}", folder, ToUpperAscii(body));
    Fitted fitted{.original = ReadRetail(vfs, stem + ".GDB", ParseGeometry),
                  .skeleton = ReadRetail(vfs, stem + ".SDB", ParseSkeleton),
                  .model = {},
                  .rig = {}};
    LdrawLibrary library(RR_LDRAW_DIR);
    std::vector<FigurePiece> pieces;
    for (const MinifigPiece& piece : MinifigFigureOf(body)) {
        Result<GeometryAsset> part = library.LoadPart(piece.file);
        REQUIRE(part.has_value());
        pieces.push_back({.piece = &piece, .geometry = std::move(part).value_or(GeometryAsset{})});
    }
    Result<FittedMinifig> model = FitMinifigBody(fitted.original, fitted.skeleton, pieces);
    REQUIRE(model.has_value());
    if (!model) {
        return std::nullopt;
    }
    fitted.model = std::move(model->geometry);
    fitted.rig = std::move(model->rig);
    return fitted;
}

std::set<std::string> JointsOf(const Fitted& fitted, std::string_view material) {
    std::set<std::string> joints;
    for (const GeometryRange& range : fitted.model.ranges) {
        if (fitted.model.materials.at(range.material) != material) {
            continue;
        }
        for (std::uint32_t index = range.firstIndex; index < range.firstIndex + range.indexCount;
             ++index) {
            const GeometryVertex& vertex = fitted.model.vertices.at(fitted.model.indices.at(index));
            joints.insert(fitted.skeleton.nodes.at(vertex.joint).name);
        }
    }
    return joints;
}

std::vector<std::pair<std::uint16_t, std::uint32_t>> FaceRanges(const GeometryAsset& model) {
    std::vector<std::pair<std::uint16_t, std::uint32_t>> faces;
    for (const GeometryRange& range : model.ranges) {
        if (model.materials.at(range.material) == "face") {
            faces.emplace_back(range.joint, range.indexCount);
        }
    }
    return faces;
}

bool IsBoundAndMapped(const GeometryVertex& vertex, std::size_t joints) {
    return vertex.joint < joints && vertex.u >= 0.0F && vertex.u <= 1.0F && vertex.v >= 0.0F &&
           vertex.v <= 1.0F;
}

void CheckCommon(const Fitted& fitted) {
    CHECK(fitted.model.scale == fitted.original.scale);
    CHECK(FaceRanges(fitted.model) == FaceRanges(fitted.original));
    CHECK(std::set(fitted.model.materials.begin(), fitted.model.materials.end()) ==
          std::set<std::string>{"face", "torso", "legs"});
    CHECK(std::ranges::all_of(fitted.model.vertices, [&](const GeometryVertex& vertex) {
        return IsBoundAndMapped(vertex, fitted.skeleton.nodes.size());
    }));
    CHECK(fitted.model.indices.size() > fitted.original.indices.size() * 4);
}

bool HasPivots(const Fitted& fitted) {
    return std::ranges::all_of(fitted.rig.limbs, [&](const RigLimb& limb) {
        return std::ranges::all_of(limb.nodes, [&](std::size_t node) {
            return std::ranges::contains(fitted.rig.pivots, node, &RigPivot::node);
        });
    });
}

bool HasJointsStarting(const std::set<std::string>& joints,
                       std::initializer_list<std::string_view> prefixes) {
    return std::ranges::all_of(prefixes, [&](std::string_view prefix) {
        return std::ranges::any_of(
            joints, [&](const std::string& joint) { return joint.starts_with(prefix); });
    });
}

void CheckStanding(const Fitted& fitted, std::string_view body) {
    CheckCommon(fitted);
    CHECK(HasJointsStarting(JointsOf(fitted, "torso"),
                            {"chest", "l_sh", "l_el", "r_sh", "r_el", "r_hand"}));
    CHECK_FALSE(HasJointsStarting(JointsOf(fitted, "torso"), {"waist"}));
    CHECK(HasJointsStarting(JointsOf(fitted, "legs"),
                            {body, "l_th", "l_kn", "l_an", "r_th", "r_kn", "r_an"}));
    CHECK(fitted.rig.limbs.size() == 4);
    CHECK(HasPivots(fitted));
}

TEST_CASE("standing bodies rig LDraw arms and legs to bend at their own elbows and knees") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    for (const std::string_view body : {"rr", "hr", "rp", "hp"}) {
        CAPTURE(body);
        const std::optional<Fitted> fitted = FitRetail(*vfs, kMenuFolder, body);
        if (fitted) {
            CheckStanding(*fitted, body);
        }
    }
}

TEST_CASE("the race body seats LDraw legs on the leg box and arms on their joints") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    const std::optional<Fitted> fitted = FitRetail(*vfs, kRaceFolder, "leg_box");
    if (!fitted) {
        return;
    }
    CheckCommon(*fitted);
    CHECK(JointsOf(*fitted, "torso") == std::set<std::string>{"chest", "arm_l", "arm_r"});
    CHECK(JointsOf(*fitted, "legs") == std::set<std::string>{"leg_box"});
    CHECK(fitted->rig.limbs.empty());
}

bool IsSwapped(const PartGroup& before, const PartGroup& after, std::size_t headTriangles) {
    const std::size_t expected =
        before.material == "face" ? headTriangles : before.triangles.size();
    return after.material == before.material && after.triangles.size() == expected;
}

bool AreHeadsSwapped(const LibraryPart& before, const LibraryPart& after,
                     std::size_t headTriangles) {
    return std::ranges::equal(before.groups, after.groups,
                              [&](const PartGroup& was, const PartGroup& is) {
                                  return IsSwapped(was, is, headTriangles);
                              });
}

void CheckHeadsSwapped(const PartLibrary& original, const PartLibrary& fitted,
                       std::size_t headTriangles) {
    CHECK(std::ranges::equal(original.parts, fitted.parts,
                             [&](const LibraryPart& before, const LibraryPart& after) {
                                 return AreHeadsSwapped(before, after, headTriangles);
                             }));
}

TEST_CASE("every head with a face gets the LDraw head and keeps its hat") {
    const std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return;
    }
    LdrawLibrary ldraw(RR_LDRAW_DIR);
    const GeometryAsset head = ldraw.LoadPart(kMinifigHeadFile).value_or(GeometryAsset{});
    REQUIRE_FALSE(head.indices.empty());
    for (const std::string& path : {std::format("{}/CBBODIES.GCB", kMenuFolder),
                                    std::format("{}/ICB_CHAR.GCB", kRaceFolder)}) {
        CAPTURE(path);
        const PartLibrary original = ReadRetail(*vfs, path, ParsePartLibrary);
        const Result<PartLibrary> fitted = WritePartLibrary(FitMinifigHeads(original, head))
                                               .and_then([](const std::vector<std::byte>& bytes) {
                                                   return ParsePartLibrary(bytes);
                                               });
        REQUIRE(fitted.has_value());
        CheckHeadsSwapped(original, fitted.value_or(PartLibrary{}), head.indices.size() / 3);
    }
}

}
}
