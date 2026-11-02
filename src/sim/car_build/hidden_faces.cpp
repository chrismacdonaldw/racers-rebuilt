#include "sim/car_build/hidden_faces.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace rr::sim {
namespace {

constexpr float kPlaneTolerance = 0.005F;
constexpr int kQuadTurns = 4;

enum class Axis : std::uint8_t { X, Y, Z };

float& On(Vector3& point, Axis axis) {
    switch (axis) {
        case Axis::X:
            return point.x;
        case Axis::Y:
            return point.y;
        default:
            return point.z;
    }
}

float On(const Vector3& point, Axis axis) {
    switch (axis) {
        case Axis::X:
            return point.x;
        case Axis::Y:
            return point.y;
        default:
            return point.z;
    }
}

struct Bounds {
    Vector3 min;
    Vector3 max;
};

Bounds BoundsOf(const MeshPrimitives& primitives, const MeshPrimitive& primitive) {
    const Vector3& first = primitives.Vertex(primitive.corners[0]).position;
    Bounds bounds{.min = first, .max = first};
    for (std::size_t corner = 1; corner < primitive.cornerCount; ++corner) {
        const Vector3& point = primitives.Vertex(primitive.corners[corner]).position;
        for (const Axis axis : {Axis::X, Axis::Y, Axis::Z}) {
            On(bounds.min, axis) = std::min(On(bounds.min, axis), On(point, axis));
            On(bounds.max, axis) = std::max(On(bounds.max, axis), On(point, axis));
        }
    }
    return bounds;
}

bool Overlaps(const Bounds& left, const Bounds& right, Axis axis) {
    return On(left.min, axis) < On(right.max, axis) && On(right.min, axis) < On(left.max, axis);
}

void Turn(MeshPrimitive& quad) {
    const std::array<std::uint32_t, 4> was = quad.corners;
    quad.corners = {was[1], was[3], was[0], was[2]};
}

std::int8_t Blend(std::int8_t low, std::int8_t high, float lowScale, float highScale) {
    return static_cast<std::int8_t>((static_cast<float>(low) * lowScale) +
                                    (static_cast<float>(high) * highScale));
}

void Interpolate(MeshVertex& split, const MeshVertex& low, const MeshVertex& high, float along) {
    const float stay = 1.0F - along;
    split.u = (low.u * stay) + (high.u * along);
    split.v = (low.v * stay) + (high.v * along);
    for (std::size_t axis = 0; axis < split.normal.size(); ++axis) {
        split.normal[axis] = Blend(low.normal[axis], high.normal[axis], stay, along);
    }
}

struct Plane {
    std::uint8_t flags = 0;
    Axis normal = Axis::Z;
    Axis first = Axis::Y;
    Axis second = Axis::X;
    bool isTolerant = false;
};

constexpr std::array kPlanes{
    Plane{.flags = kParallelogram | kSameZ | kOnLattice,
          .normal = Axis::Z,
          .first = Axis::Y,
          .second = Axis::X,
          .isTolerant = true},
    Plane{.flags = kParallelogram | kSameY | kOnLattice,
          .normal = Axis::Y,
          .first = Axis::Z,
          .second = Axis::X},
    Plane{.flags = kParallelogram | kSameX | kOnLattice,
          .normal = Axis::X,
          .first = Axis::Z,
          .second = Axis::Y},
};

enum class Side : std::uint8_t { Low, High };

class PlaneClipper {
public:
    PlaneClipper(MeshPrimitives& primitives, const Plane& plane)
        : primitives_(&primitives), plane_(plane) {}

    void Run() {
        Gather();
        for (left_ = 0; left_ + 1 < planes_ && !primitives_->HasOverflowed(); ++left_) {
            leftBounds_ = BoundsOf(*primitives_, primitives_->At(left_));
            for (right_ = left_ + 1; right_ < planes_; ++right_) {
                rightBounds_ = BoundsOf(*primitives_, primitives_->At(right_));
                if (Touches() && !ClipPair()) {
                    return;
                }
            }
        }
    }

private:
    void Gather() {
        planes_ = 0;
        for (std::size_t slot = 0; slot < primitives_->Live(); ++slot) {
            const MeshPrimitive& primitive = primitives_->At(slot);
            if ((primitive.flags & plane_.flags) == plane_.flags && primitive.cornerCount == 4) {
                primitives_->Swap(slot, planes_++);
            }
        }
    }

    [[nodiscard]] bool Touches() const {
        const float apart =
            On(leftBounds_.min, plane_.normal) - On(rightBounds_.min, plane_.normal);
        const bool isLevel =
            plane_.isTolerant ? -kPlaneTolerance < apart && apart < kPlaneTolerance : apart == 0.0F;
        return isLevel && primitives_->At(left_).part != primitives_->At(right_).part &&
               Overlaps(leftBounds_, rightBounds_, plane_.first) &&
               Overlaps(leftBounds_, rightBounds_, plane_.second);
    }

    bool ClipPair() {
        const bool isClipped = Clip(plane_.first, plane_.second, Side::Low) &&
                               Clip(plane_.first, plane_.second, Side::High) &&
                               Clip(plane_.second, plane_.first, Side::Low) &&
                               Clip(plane_.second, plane_.first, Side::High);
        if (primitives_->HasOverflowed()) {
            return false;
        }
        if (isClipped) {
            primitives_->Remove(right_, --planes_);
            primitives_->Remove(left_, --planes_);
            right_ = left_;
            leftBounds_ = BoundsOf(*primitives_, primitives_->At(left_));
        }
        return true;
    }

    [[nodiscard]] static float& Edge(Bounds& bounds, Axis axis, Side side) {
        return On(side == Side::Low ? bounds.min : bounds.max, axis);
    }

    bool FaceEdge(MeshPrimitive& quad, Axis axis, Side side) {
        const std::array<std::size_t, 2> edge =
            side == Side::Low ? std::array<std::size_t, 2>{0, 1} : std::array<std::size_t, 2>{2, 3};
        const float wanted = Edge(leftBounds_, axis, side);
        for (int turn = 0; turn < kQuadTurns; ++turn) {
            if (On(primitives_->Vertex(quad.corners[edge[0]]).position, axis) == wanted &&
                On(primitives_->Vertex(quad.corners[edge[1]]).position, axis) == wanted) {
                return true;
            }
            Turn(quad);
        }
        return false;
    }

    bool Clip(Axis axis, Axis across, Side side) {
        const float leftEdge = Edge(leftBounds_, axis, side);
        const float rightEdge = Edge(rightBounds_, axis, side);
        if (leftEdge == rightEdge) {
            return true;
        }
        if ((side == Side::Low) == (rightEdge < leftEdge)) {
            primitives_->Swap(left_, right_);
            std::swap(leftBounds_, rightBounds_);
        }
        MeshPrimitive& quad = primitives_->At(left_);
        if (!FaceEdge(quad, axis, side)) {
            return false;
        }
        MeshPrimitive* outside = primitives_->Duplicate(quad, planes_++);
        if (outside == nullptr) {
            return false;
        }
        Split(quad, *outside, axis, across, side);
        return true;
    }

    void Split(MeshPrimitive& quad, MeshPrimitive& outside, Axis axis, Axis across, Side side) {
        const float low = On(leftBounds_.min, axis);
        const float cut = Edge(rightBounds_, axis, side);
        const float along = (cut - low) / (On(leftBounds_.max, axis) - low);
        Edge(leftBounds_, axis, side) = cut;
        const MeshVertex firstLow = primitives_->Vertex(quad.corners[0]);
        const MeshVertex firstHigh = primitives_->Vertex(quad.corners[2]);
        MeshVertex split{.position = firstLow.position};
        On(split.position, axis) = cut;
        Interpolate(split, firstLow, firstHigh, along);
        const std::uint32_t first = primitives_->AddVertex(split);
        const MeshVertex secondLow = primitives_->Vertex(quad.corners[1]);
        const MeshVertex secondHigh = primitives_->Vertex(quad.corners[3]);
        On(split.position, across) = On(secondLow.position, across);
        Interpolate(split, secondLow, secondHigh, along);
        const std::uint32_t second = primitives_->AddVertex(split);
        const std::size_t kept = side == Side::Low ? 0 : 2;
        const std::size_t cutAway = side == Side::Low ? 2 : 0;
        outside.corners[cutAway] = first;
        outside.corners[cutAway + 1] = second;
        quad.corners[kept] = first;
        quad.corners[kept + 1] = second;
    }

    MeshPrimitives* primitives_;
    Plane plane_;
    std::size_t planes_ = 0;
    std::size_t left_ = 0;
    std::size_t right_ = 0;
    Bounds leftBounds_;
    Bounds rightBounds_;
};

}

void RemoveTouchingFaces(MeshPrimitives& primitives) {
    for (const Plane& plane : kPlanes) {
        PlaneClipper(primitives, plane).Run();
        if (primitives.HasOverflowed()) {
            return;
        }
    }
}

}
