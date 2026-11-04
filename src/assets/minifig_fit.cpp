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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/game_path.hpp"
#include "assets/ldraw_part.hpp"
#include "assets/surface_transfer.hpp"
#include "core/transform.hpp"

namespace rr::assets {
namespace {

constexpr std::string_view kFaceMaterial = "face";
constexpr std::size_t kTipCorners = 8;
constexpr float kParallel = 1e-6F;

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
                       .surface = std::move(surface)};
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

Quaternion Between(const Vector3& from, const Vector3& to) {
    const Vector3 a = Normalized(from, {});
    const Vector3 b = Normalized(to, {});
    const Vector3 axis = Cross(a, b);
    const float sine = Length(axis);
    if (sine < kParallel) {
        return {};
    }
    return FromAxisAngle(axis * (1.0F / sine), std::atan2(sine, Dot(a, b)));
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
                   const SkeletonAsset& skeleton, const Vector3& anchor) {
    const std::optional<std::size_t> joint = JointNamed(skeleton, swingJoint);
    if (!joint || *joint >= rest.joints.size()) {
        return Fail(ErrorCode::InvalidData, std::format("the body has no {} joint", swingJoint));
    }
    const Vector3 shoulder = rest.joints[*joint].position;
    std::optional<Vector3> pivot;
    std::vector<Vector3> figurePoints;
    std::vector<Vector3> bodyPoints;
    for (const PlacedPiece& placed : pieces) {
        if (placed.piece->swingJoint != swingJoint) {
            continue;
        }
        if (!pivot) {
            pivot = FigurePoint(placed.piece->placement.position, anchor);
        }
        for (const FigureVertex& vertex : placed.vertices) {
            figurePoints.push_back(vertex.position);
        }
        for (const SurfaceTriangle& triangle : placed.surface) {
            bodyPoints.insert(bodyPoints.end(), triangle.corners.begin(), triangle.corners.end());
        }
    }
    if (!pivot) {
        return {};
    }
    const Quaternion turn =
        Between(TipOf(figurePoints, *pivot) - *pivot, TipOf(bodyPoints, shoulder) - shoulder);
    for (PlacedPiece& placed : pieces) {
        if (placed.piece->swingJoint != swingJoint) {
            continue;
        }
        for (FigureVertex& vertex : placed.vertices) {
            vertex.position = shoulder + Rotate(turn, vertex.position - *pivot);
            vertex.normal = Rotate(turn, vertex.normal);
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

std::uint16_t FollowedJoint(const SkeletonAsset& skeleton, const MinifigPiece& piece) {
    if (!piece.swingJoint.empty()) {
        if (const std::optional<std::size_t> joint = JointNamed(skeleton, piece.swingJoint)) {
            return static_cast<std::uint16_t>(*joint);
        }
    }
    for (const std::string_view prefix : piece.joints) {
        if (const std::optional<std::size_t> joint = JointNamed(skeleton, prefix)) {
            return static_cast<std::uint16_t>(*joint);
        }
    }
    return 0;
}

using Buckets = std::map<std::pair<std::uint32_t, std::uint16_t>, std::vector<GeometryVertex>>;

void Bind(const PlacedPiece& placed, const Rest& rest, const SkeletonAsset& skeleton,
          std::uint32_t material, Buckets& buckets) {
    const std::uint16_t joint = FollowedJoint(skeleton, *placed.piece);
    const Transform undo = Inverse(JointAt(rest.joints, joint));
    std::vector<GeometryVertex>& bucket = buckets[{material, joint}];
    for (std::size_t first = 0; first + 2 < placed.indices.size(); first += 3) {
        std::array<const FigureVertex*, 3> corners{};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            corners.at(corner) = &placed.vertices.at(placed.indices[first + corner]);
        }
        const Vector3 facing = Normalized(Cross(corners[1]->position - corners[0]->position,
                                                corners[2]->position - corners[0]->position),
                                          {});
        if (Dot(facing, facing) == 0.0F) {
            continue;
        }
        const std::array<SurfacePoint, 3> sources = NearestSurfacePoints(
            placed.surface, {corners[0]->position, corners[1]->position, corners[2]->position},
            facing);
        for (std::size_t corner = 0; corner < 3; ++corner) {
            bucket.push_back({.position = Apply(undo, corners.at(corner)->position),
                              .normal = Rotate(undo.rotation, corners.at(corner)->normal),
                              .u = sources.at(corner).u,
                              .v = sources.at(corner).v,
                              .rgba = kOpaqueWhite,
                              .joint = joint});
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

Result<GeometryAsset> FitMinifigBody(const GeometryAsset& body, const SkeletonAsset& skeleton,
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
    for (const std::string_view joint : SwingJoints(pieces)) {
        if (Result<void> swung = Swing(placed, joint, rest, skeleton, anchor); !swung) {
            return std::unexpected(std::move(swung.error()));
        }
    }
    std::vector<std::string> materials;
    Buckets buckets;
    KeepFace(body, FindOrAddMaterial(materials, std::string(kFaceMaterial)), buckets);
    for (const PlacedPiece& piece : placed) {
        Bind(piece, rest, skeleton,
             FindOrAddMaterial(materials, std::string(piece.piece->material)), buckets);
    }
    return Assemble(buckets, std::move(materials), body.scale);
}

}
