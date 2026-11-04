#include "sim/animation/figure_retarget.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "sim/animation/skeleton_pose.hpp"

namespace rr::sim {
namespace {

constexpr float kStraightest = 0.999F;
constexpr float kShortest = 0.001F;

Quaternion TurnOf(const Transform& posed, const Transform& rest) {
    return posed.rotation * Conjugate(rest.rotation);
}

Transform TurnedAbout(const Transform& rest, const Quaternion& turn, const Vector3& pivot,
                      const Vector3& placed) {
    return {.position = placed + Rotate(turn, rest.position - pivot),
            .rotation = turn * rest.rotation};
}

Vector3 Across(const Vector3& v, const Vector3& along) {
    return v - (along * Dot(v, along));
}

struct Bent {
    Vector3 elbow;
    Vector3 wrist;
};

struct Lengths {
    float upper = 0.0F;
    float lower = 0.0F;
};

Bent Reach(const Vector3& base, const Vector3& target, const Vector3& pole,
           const Lengths& lengths) {
    const Vector3 toTarget = target - base;
    const Vector3 along = Normalized(toTarget, Normalized(pole, {.z = -1.0F}));
    const float longest = (lengths.upper + lengths.lower) * kStraightest;
    const float shortest = std::min(std::abs(lengths.upper - lengths.lower) + kShortest, longest);
    const float distance = std::clamp(Length(toTarget), shortest, longest);
    const float cosine = std::clamp(((lengths.upper * lengths.upper) + (distance * distance) -
                                     (lengths.lower * lengths.lower)) /
                                        (2.0F * lengths.upper * distance),
                                    -1.0F, 1.0F);
    const Vector3 side = Normalized(Across(pole, along), {});
    return {.elbow = base + (along * (lengths.upper * cosine)) +
                     (side * (lengths.upper * std::sqrt(1.0F - (cosine * cosine)))),
            .wrist = base + (along * distance)};
}

Quaternion TwistAbout(const Quaternion& turn, const Vector3& axis) {
    const Vector3 along = axis * Dot({.x = turn.x, .y = turn.y, .z = turn.z}, axis);
    const float length = std::sqrt((turn.w * turn.w) + Dot(along, along));
    if (length < kShortest) {
        return {};
    }
    const float unit = 1.0F / length;
    return {.w = turn.w * unit, .x = along.x * unit, .y = along.y * unit, .z = along.z * unit};
}

class Retarget {
public:
    Retarget(const assets::SkeletonAsset& skeleton, const assets::FigureRig& rig, float scale,
             std::span<const Transform> posed, std::span<Transform> next)
        : skeleton_(&skeleton), rig_(&rig), scale_(scale), posed_(posed), next_(next) {}

    void Run() {
        for (std::size_t node = 0; node < next_.size(); ++node) {
            if (const assets::RigLimb* limb = LimbFrom(node)) {
                Bend(*limb);
            } else if (IsLimbTail(node)) {
                continue;
            } else if (const std::optional<Vector3> pivot = PivotOf(node)) {
                Pivot(node, *pivot);
            } else {
                Follow(node);
            }
        }
    }

private:
    [[nodiscard]] bool IsInFigure(const assets::RigLimb& limb) const {
        return std::ranges::all_of(
            limb.nodes, [&](std::size_t node) { return node < next_.size() && PivotOf(node); });
    }

    [[nodiscard]] const assets::RigLimb* LimbFrom(std::size_t node) const {
        const auto limb = std::ranges::find_if(rig_->limbs, [&](const assets::RigLimb& found) {
            return found.nodes[0] == node && IsInFigure(found);
        });
        return limb == rig_->limbs.end() ? nullptr : &*limb;
    }

    [[nodiscard]] bool IsLimbTail(std::size_t node) const {
        return std::ranges::any_of(rig_->limbs, [&](const assets::RigLimb& limb) {
            return (limb.nodes[1] == node || limb.nodes[2] == node) && limb.nodes[0] < node &&
                   IsInFigure(limb);
        });
    }

    [[nodiscard]] std::optional<Vector3> PivotOf(std::size_t node) const {
        const auto pivot = std::ranges::find(rig_->pivots, node, &assets::RigPivot::node);
        return pivot == rig_->pivots.end() ? std::nullopt : std::optional(pivot->position * scale_);
    }

    [[nodiscard]] Transform Rest(std::size_t node) const {
        const Transform& bind = rig_->bind[node];
        return {.position = bind.position * scale_, .rotation = bind.rotation};
    }

    [[nodiscard]] std::optional<std::size_t> ParentOf(std::size_t node) const {
        const std::optional<std::size_t> parent = skeleton_->nodes[node].parent;
        return parent && *parent < node ? parent : std::nullopt;
    }

    [[nodiscard]] Vector3 Carried(std::size_t node, const Vector3& point) const {
        const std::optional<std::size_t> parent = ParentOf(node);
        return parent ? Apply(Compose(next_[*parent], Inverse(Rest(*parent))), point) : point;
    }

    [[nodiscard]] Quaternion Turn(std::size_t node) const {
        return TurnOf(posed_[node], Rest(node));
    }

    void Follow(std::size_t node) {
        if (const std::optional<std::size_t> parent = ParentOf(node)) {
            next_[node] = Compose(Compose(next_[*parent], Inverse(posed_[*parent])), posed_[node]);
        }
    }

    void Pivot(std::size_t node, const Vector3& pivot) {
        next_[node] = TurnedAbout(Rest(node), Turn(node), pivot, Carried(node, pivot));
    }

    void Bend(const assets::RigLimb& limb) {
        const auto [upper, middle, end] = limb.nodes;
        const Vector3 shoulder = *PivotOf(upper);
        const Vector3 elbow = *PivotOf(middle);
        const Vector3 wrist = *PivotOf(end);
        const Quaternion lowerRest =
            RotationBetween(wrist - elbow, Rest(end).position - Rest(middle).position);
        const Quaternion upperTurn =
            Turn(upper) *
            RotationBetween(elbow - shoulder, Rest(middle).position - Rest(upper).position);
        const Quaternion lowerTurn = Turn(middle) * lowerRest;
        const Vector3 base = Carried(upper, shoulder);
        const Bent bent =
            Reach(base, posed_[end].position, posed_[middle].position - base,
                  {.upper = Length(elbow - shoulder), .lower = Length(wrist - elbow)});
        const Quaternion upperBend =
            RotationBetween(Rotate(upperTurn, elbow - shoulder), bent.elbow - base) * upperTurn;
        const Quaternion lowerBend =
            RotationBetween(Rotate(lowerTurn, wrist - elbow), bent.wrist - bent.elbow);
        next_[upper] = TurnedAbout(Rest(upper), upperBend, shoulder, base);
        next_[middle] = TurnedAbout(Rest(middle), lowerBend * lowerTurn, elbow, bent.elbow);
        const Quaternion wristTwist = TwistAbout(Turn(end) * Conjugate(Turn(middle)),
                                                 Normalized(Rotate(lowerTurn, wrist - elbow), {}));
        next_[end] = TurnedAbout(Rest(end), lowerBend * wristTwist * lowerTurn, wrist, bent.wrist);
    }

    const assets::SkeletonAsset* skeleton_;
    const assets::FigureRig* rig_;
    float scale_;
    std::span<const Transform> posed_;
    std::span<Transform> next_;
};

}

void FigureRetargeter::Run(const assets::SkeletonAsset& skeleton, float scale,
                           std::span<Transform> models) {
    const std::optional<assets::FigureRig>& rig = skeleton.rig;
    if (!rig || scale <= 0.0F || models.size() != skeleton.nodes.size() ||
        rig->bind.size() != models.size()) {
        return;
    }
    posed_.assign(models.begin(), models.end());
    Retarget(skeleton, *rig, scale, posed_, models).Run();
}

assets::FigureRig BoundRig(const assets::FigureRig& rig, const assets::SkeletonAsset& body) {
    SkeletonPose pose(body, 1.0F);
    const std::span<const Transform> rest = pose.Solve();
    assets::FigureRig bound = rig;
    bound.bind.assign(rest.begin(), rest.end());
    return bound;
}

}
