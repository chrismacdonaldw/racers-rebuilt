#include "assets/minifig_fit.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "assets/ldraw_part.hpp"
#include "assets/minifig_skin.hpp"
#include "assets/surface_transfer.hpp"
#include "core/transform.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kFaceMaterial = "face";
constexpr std::size_t kTipCorners = 8;
constexpr float kRootBand = 0.1F;
constexpr std::array kElbowCut{0.5F};
constexpr std::array kKneeAnkleCuts{0.54F, 0.73F};

using Rows = std::array<Vector3, 3>;

constexpr Rows kLoadedToLdraw{Vector3{.x = 1.0F}, Vector3{.z = -1.0F}, Vector3{.y = 1.0F}};
constexpr Rows kLdrawToFigure{Vector3{.z = -1.0F}, Vector3{.x = 1.0F}, Vector3{.y = -1.0F}};

constexpr Vector3 Times(const Rows& rows, const Vector3& v) {
    return {.x = Dot(rows[0], v), .y = Dot(rows[1], v), .z = Dot(rows[2], v)};
}

struct FigureVertex {
    Vector3 position;
    Vector3 normal;
};

struct PlacedPiece {
    const MinifigPiece* piece = nullptr;
    std::vector<FigureVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<SurfaceTriangle> surface;
    Vector3 pivot;
    Transform lookup;
    std::vector<std::uint16_t> segments;
    Vector3 root;
    Vector3 axis;
    float length = 0.0F;
};

struct Rest {
    std::vector<Transform> joints;
    std::vector<SurfaceTriangle> surface;
};

std::vector<Transform> RestPose(const SkeletonAsset& skeleton, float scale) {
    std::vector<Transform> world;
    world.reserve(skeleton.nodes.size());
    for (const SkeletonNode& node : skeleton.nodes) {
        const Transform local{.position = node.position * scale, .rotation = node.rotation};
        world.push_back(node.parent && *node.parent < world.size()
                            ? Compose(world[*node.parent], local)
                            : local);
    }
    return world;
}

Transform JointAt(const std::vector<Transform>& joints, std::uint16_t joint) {
    return joint < joints.size() ? joints[joint] : Transform{};
}

Rest RestOf(const GeometryAsset& body, const SkeletonAsset& skeleton) {
    Rest rest{.joints = RestPose(skeleton, body.scale), .surface = {}};
    for (const GeometryRange& range : body.ranges) {
        const std::uint32_t end = range.firstIndex + range.indexCount;
        for (std::uint32_t first = range.firstIndex; first + 2 < end; first += 3) {
            std::array<Vector3, 3> corners;
            std::array<const GeometryVertex*, 3> vertices{};
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const GeometryVertex& vertex = body.vertices.at(body.indices.at(first + corner));
                vertices.at(corner) = &vertex;
                corners.at(corner) = Apply(JointAt(rest.joints, vertex.joint), vertex.position);
            }
            SurfaceTriangle& triangle = rest.surface.emplace_back(MakeSurfaceTriangle(corners));
            for (std::size_t corner = 0; corner < 3; ++corner) {
                triangle.u.at(corner) = vertices.at(corner)->u;
                triangle.v.at(corner) = vertices.at(corner)->v;
            }
            triangle.joint = range.joint;
            triangle.material = range.material;
        }
    }
    return rest;
}

bool IsJoint(const SkeletonAsset& skeleton, std::size_t joint, std::string_view prefix) {
    if (prefix == kRootJoint) {
        return joint == 0;
    }
    return !prefix.empty() && joint < skeleton.nodes.size() &&
           ToUpperAscii(skeleton.nodes[joint].name).starts_with(ToUpperAscii(prefix));
}

bool IsPieceJoint(const SkeletonAsset& skeleton, std::size_t joint, const MinifigPiece& piece) {
    return std::ranges::any_of(
        piece.joints, [&](std::string_view prefix) { return IsJoint(skeleton, joint, prefix); });
}

std::vector<SurfaceTriangle> SurfaceOf(const Rest& rest, const GeometryAsset& body,
                                       const SkeletonAsset& skeleton, const MinifigPiece& piece) {
    std::vector<SurfaceTriangle> surface;
    std::ranges::copy_if(rest.surface, std::back_inserter(surface),
                         [&](const SurfaceTriangle& triangle) {
                             return body.materials.at(triangle.material) == piece.material &&
                                    IsPieceJoint(skeleton, triangle.joint, piece);
                         });
    return surface;
}

Vector3 FigurePoint(const Vector3& ldraw, const Vector3& anchor) {
    return (Times(kLdrawToFigure, ldraw) * kGameUnitsPerLdu) + anchor;
}

PlacedPiece Place(const FigurePiece& figure, const Vector3& anchor,
                  std::vector<SurfaceTriangle> surface) {
    const LdrawMatrix& placement = figure.piece->placement;
    const LdrawMatrix turn{.position = {}, .rows = placement.rows};
    PlacedPiece placed{.piece = figure.piece,
                       .vertices = {},
                       .indices = figure.geometry.indices,
                       .surface = std::move(surface),
                       .pivot = FigurePoint(figure.piece->pivot, anchor),
                       .lookup = {},
                       .segments = {},
                       .axis = {},
                       .length = 0.0F};
    placed.vertices.reserve(figure.geometry.vertices.size());
    for (const GeometryVertex& vertex : figure.geometry.vertices) {
        const Vector3 ldraw = Times(kLoadedToLdraw, vertex.position) * (1.0F / kGameUnitsPerLdu);
        placed.vertices.push_back(
            {.position = FigurePoint(Apply(placement, ldraw), anchor),
             .normal = Times(kLdrawToFigure, Apply(turn, Times(kLoadedToLdraw, vertex.normal)))});
    }
    return placed;
}

Vector3 TipOf(const std::vector<Vector3>& points, const Vector3& from) {
    std::vector<std::pair<float, Vector3>> distances;
    distances.reserve(points.size());
    for (const Vector3& point : points) {
        distances.emplace_back(Length(point - from), point);
    }
    const std::size_t count = std::min(kTipCorners, distances.size());
    if (count == 0) {
        return from;
    }
    std::ranges::partial_sort(distances, distances.begin() + static_cast<std::ptrdiff_t>(count),
                              std::ranges::greater{}, &std::pair<float, Vector3>::first);
    Vector3 sum;
    for (std::size_t i = 0; i < count; ++i) {
        sum += distances[i].second;
    }
    return sum * (1.0F / static_cast<float>(count));
}

Vector3 RootOf(const std::vector<Vector3>& points, const Vector3& from, const Vector3& tip) {
    const Vector3 along = Normalized(tip - from, {});
    const float band = Length(tip - from) * kRootBand;
    Vector3 sum;
    float count = 0.0F;
    for (const Vector3& point : points) {
        if (Dot(point - from, along) < band) {
            sum += point;
            count += 1.0F;
        }
    }
    return count > 0.0F ? sum * (1.0F / count) : from;
}

std::optional<std::size_t> JointNamed(const SkeletonAsset& skeleton, std::string_view prefix) {
    for (std::size_t joint = 0; joint < skeleton.nodes.size(); ++joint) {
        if (IsJoint(skeleton, joint, prefix)) {
            return joint;
        }
    }
    return std::nullopt;
}

Result<void> Swing(std::vector<PlacedPiece>& pieces, std::string_view swingJoint, const Rest& rest,
                   const SkeletonAsset& skeleton) {
    const std::optional<std::size_t> joint = JointNamed(skeleton, swingJoint);
    if (!joint || *joint >= rest.joints.size()) {
        return Fail(ErrorCode::InvalidData, std::format("the body has no {} joint", swingJoint));
    }
    const Vector3 shoulder = rest.joints[*joint].position;
    std::vector<Vector3> figurePoints;
    std::vector<Vector3> bodyPoints;
    for (const PlacedPiece& placed : pieces) {
        if (placed.piece->swingJoint != swingJoint) {
            continue;
        }
        for (const FigureVertex& vertex : placed.vertices) {
            figurePoints.push_back(vertex.position);
        }
        for (const SurfaceTriangle& triangle : placed.surface) {
            bodyPoints.insert(bodyPoints.end(), triangle.corners.begin(), triangle.corners.end());
        }
    }
    if (figurePoints.empty()) {
        return {};
    }
    const Quaternion turn = RotationBetween(TipOf(figurePoints, shoulder) - shoulder,
                                            TipOf(bodyPoints, shoulder) - shoulder);
    for (PlacedPiece& placed : pieces) {
        if (placed.piece->swingJoint == swingJoint) {
            placed.lookup = {.position = shoulder - Rotate(turn, shoulder), .rotation = turn};
        }
    }
    return {};
}

std::vector<std::string_view> SwingJoints(std::span<const FigurePiece> pieces) {
    std::vector<std::string_view> joints;
    for (const FigurePiece& figure : pieces) {
        const std::string_view joint = figure.piece->swingJoint;
        if (!joint.empty() && !std::ranges::contains(joints, joint)) {
            joints.push_back(joint);
        }
    }
    return joints;
}

std::span<const float> CutsOf(const PlacedPiece& placed) {
    switch (placed.segments.size()) {
        case kElbowCut.size() + 1:
            return kElbowCut;
        case kKneeAnkleCuts.size() + 1:
            return kKneeAnkleCuts;
        default:
            return {};
    }
}

std::span<const std::string_view> SegmentJoints(const MinifigPiece& piece) {
    return piece.segments.front().empty() ? std::span<const std::string_view>(piece.joints).first(1)
                                          : std::span<const std::string_view>(piece.segments);
}

Result<void> Segment(PlacedPiece& placed, const SkeletonAsset& skeleton) {
    for (const std::string_view prefix : SegmentJoints(*placed.piece)) {
        if (prefix.empty()) {
            continue;
        }
        const std::optional<std::size_t> joint = JointNamed(skeleton, prefix);
        if (!joint) {
            return Fail(ErrorCode::InvalidData, std::format("the body has no {} joint", prefix));
        }
        placed.segments.push_back(static_cast<std::uint16_t>(*joint));
    }
    std::vector<Vector3> points;
    points.reserve(placed.vertices.size());
    std::ranges::transform(placed.vertices, std::back_inserter(points), &FigureVertex::position);
    const Vector3 tip = TipOf(points, placed.pivot);
    placed.root = RootOf(points, placed.pivot, tip);
    const Vector3 reach = tip - placed.root;
    placed.axis = Normalized(reach, {});
    placed.length = Length(reach);
    return {};
}

std::uint16_t SegmentAt(const PlacedPiece& placed, const Vector3& point) {
    const float along = Dot(point - placed.root, placed.axis);
    const auto passed = std::ranges::count_if(
        CutsOf(placed), [&](float cut) { return along > cut * placed.length; });
    return placed.segments.at(static_cast<std::size_t>(passed));
}

Vector3 SegmentStart(const PlacedPiece& placed, std::size_t segment) {
    if (segment == 0) {
        return placed.pivot;
    }
    return placed.root + (placed.axis * (CutsOf(placed)[segment - 1] * placed.length));
}

const PlacedPiece* HandOf(std::span<const PlacedPiece> pieces, const PlacedPiece& arm) {
    const auto hand = std::ranges::find_if(pieces, [&](const PlacedPiece& placed) {
        return &placed != &arm && !arm.piece->swingJoint.empty() &&
               placed.piece->swingJoint == arm.piece->swingJoint && placed.segments.size() == 1;
    });
    return hand == pieces.end() ? nullptr : &*hand;
}

std::optional<RigLimb> LimbOf(std::span<const PlacedPiece> pieces, const PlacedPiece& placed) {
    const std::vector<std::uint16_t>& joints = placed.segments;
    if (joints.size() == kKneeAnkleCuts.size() + 1) {
        return RigLimb{.nodes = {joints[0], joints[1], joints[2]}};
    }
    const PlacedPiece* hand =
        joints.size() == kElbowCut.size() + 1 ? HandOf(pieces, placed) : nullptr;
    if (hand == nullptr) {
        return std::nullopt;
    }
    return RigLimb{.nodes = {joints[0], joints[1], hand->segments.front()}};
}

FigureRig RigOf(std::span<const PlacedPiece> pieces, float scale) {
    FigureRig rig;
    const float unscale = scale > 0.0F ? 1.0F / scale : 1.0F;
    for (const PlacedPiece& placed : pieces) {
        for (std::size_t segment = 0; segment < placed.segments.size(); ++segment) {
            const std::uint16_t joint = placed.segments[segment];
            if (joint != 0 && !std::ranges::contains(rig.pivots, joint, &RigPivot::node)) {
                rig.pivots.push_back(
                    {.node = joint, .position = SegmentStart(placed, segment) * unscale});
            }
        }
        if (std::optional<RigLimb> limb = LimbOf(pieces, placed)) {
            rig.limbs.push_back(*limb);
        }
    }
    return rig;
}

using Buckets = std::map<std::pair<std::uint32_t, std::uint16_t>, std::vector<GeometryVertex>>;

void Bind(const PlacedPiece& placed, const Rest& rest, std::uint32_t material, Buckets& buckets) {
    const JointLocator locator = [&placed](const Vector3& point) {
        return SegmentAt(placed, point);
    };
    for (std::size_t first = 0; first + 2 < placed.indices.size(); first += 3) {
        std::array<SkinCorner, 3> corners;
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const FigureVertex& vertex = placed.vertices.at(placed.indices[first + corner]);
            corners.at(corner) = {.position = vertex.position, .normal = vertex.normal};
        }
        const Vector3 facing = Normalized(Cross(corners[1].position - corners[0].position,
                                                corners[2].position - corners[0].position),
                                          {});
        if (Dot(facing, facing) == 0.0F) {
            continue;
        }
        for (const SkinnedTriangle& skinned : SkinTriangle(locator, corners)) {
            const std::array<SurfacePoint, 3> sources =
                NearestSurfacePoints(placed.surface,
                                     {Apply(placed.lookup, skinned.corners[0].position),
                                      Apply(placed.lookup, skinned.corners[1].position),
                                      Apply(placed.lookup, skinned.corners[2].position)},
                                     Rotate(placed.lookup.rotation, facing));
            std::vector<GeometryVertex>& bucket = buckets[{material, skinned.joints[0]}];
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const Transform undo = Inverse(JointAt(rest.joints, skinned.joints.at(corner)));
                bucket.push_back(
                    {.position = Apply(undo, skinned.corners.at(corner).position),
                     .normal = Rotate(undo.rotation, skinned.corners.at(corner).normal),
                     .u = sources.at(corner).u,
                     .v = sources.at(corner).v,
                     .rgba = kOpaqueWhite,
                     .joint = skinned.joints.at(corner)});
            }
        }
    }
}

void KeepFace(const GeometryAsset& body, std::uint32_t material, Buckets& buckets) {
    for (const GeometryRange& range : body.ranges) {
        if (body.materials.at(range.material) != kFaceMaterial) {
            continue;
        }
        std::vector<GeometryVertex>& bucket = buckets[{material, range.joint}];
        for (std::uint32_t index = range.firstIndex; index < range.firstIndex + range.indexCount;
             ++index) {
            bucket.push_back(body.vertices.at(body.indices.at(index)));
        }
    }
}

GeometryAsset Assemble(Buckets& buckets, std::vector<std::string> materials, float scale) {
    GeometryAsset model{.vertices = {},
                        .indices = {},
                        .materials = std::move(materials),
                        .ranges = {},
                        .scale = scale};
    for (auto& [key, vertices] : buckets) {
        const auto first = static_cast<std::uint32_t>(model.indices.size());
        for (const GeometryVertex& vertex : vertices) {
            model.indices.push_back(static_cast<std::uint32_t>(model.vertices.size()));
            model.vertices.push_back(vertex);
        }
        model.ranges.push_back({.firstIndex = first,
                                .indexCount = static_cast<std::uint32_t>(vertices.size()),
                                .material = key.first,
                                .joint = key.second});
    }
    return model;
}

}

Result<FittedMinifig> FitMinifigBody(const GeometryAsset& body, const SkeletonAsset& skeleton,
                                     std::span<const FigurePiece> pieces) {
    if (pieces.empty()) {
        return Fail(ErrorCode::InvalidData, "a minifig needs pieces");
    }
    const Rest rest = RestOf(body, skeleton);
    std::vector<std::vector<SurfaceTriangle>> surfaces;
    for (const FigurePiece& figure : pieces) {
        surfaces.push_back(SurfaceOf(rest, body, skeleton, *figure.piece));
        if (surfaces.back().empty()) {
            return Fail(ErrorCode::InvalidData,
                        std::format("the body has no {} triangles for {}", figure.piece->material,
                                    figure.piece->file));
        }
    }
    float top = std::numeric_limits<float>::lowest();
    for (const SurfaceTriangle& triangle : surfaces.front()) {
        for (const Vector3& corner : triangle.corners) {
            top = std::max(top, corner.z);
        }
    }
    const Vector3 anchor{.z = top};
    std::vector<PlacedPiece> placed;
    placed.reserve(pieces.size());
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        placed.push_back(Place(pieces[i], anchor, std::move(surfaces[i])));
    }
    for (PlacedPiece& piece : placed) {
        if (Result<void> segmented = Segment(piece, skeleton); !segmented) {
            return std::unexpected(std::move(segmented.error()));
        }
    }
    for (const std::string_view joint : SwingJoints(pieces)) {
        if (Result<void> swung = Swing(placed, joint, rest, skeleton); !swung) {
            return std::unexpected(std::move(swung.error()));
        }
    }
    std::vector<std::string> materials;
    Buckets buckets;
    KeepFace(body, FindOrAddMaterial(materials, std::string(kFaceMaterial)), buckets);
    for (const PlacedPiece& piece : placed) {
        Bind(piece, rest, FindOrAddMaterial(materials, std::string(piece.piece->material)),
             buckets);
    }
    return FittedMinifig{.geometry = Assemble(buckets, std::move(materials), body.scale),
                         .rig = RigOf(placed, body.scale)};
}

}
