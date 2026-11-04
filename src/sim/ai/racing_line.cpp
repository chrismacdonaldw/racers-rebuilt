#include "sim/ai/racing_line.hpp"

#include <algorithm>
#include <optional>

#include "core/quaternion.hpp"

namespace rr::sim {
namespace {

constexpr float kHalfCarWidth = 2.5F;
constexpr float kProbeLift = 1.0F;
constexpr int kSmoothingPasses = 150;
constexpr int kWallRounds = 3;
constexpr float kWallPullBack = 0.5F;
constexpr float kLoopJoin = 20.0F;

Vector3 Flat(const Vector3& v) {
    return {.x = v.x, .y = v.y};
}

Vector3 LeftOf(const Quaternion& rotation) {
    const Vector3 forward = Normalized(Flat(Rotate(rotation, {.x = 1.0F})), {.x = 1.0F});
    return {.x = -forward.y, .y = forward.x};
}

struct Corridor {
    std::vector<Vector3> centres;
    std::vector<Vector3> lefts;
    std::vector<float> lows;
    std::vector<float> highs;
    std::vector<bool> isFixed;
};

float ClearWidth(const CollisionQuery& world, const Vector3& from, const Vector3& side,
                 float width) {
    if (width <= 0.0F) {
        return 0.0F;
    }
    const std::optional<SegmentHit> hit = CastSolid(world, from, from + (side * width));
    return hit ? hit->fraction * width : width;
}

Corridor CorridorOf(const assets::RaceRoute& route, const CollisionQuery& world,
                    std::size_t& walledPoints) {
    Corridor corridor;
    const std::size_t count = route.points.size();
    Vector3 position = route.startPosition;
    for (std::size_t i = 0; i < count; ++i) {
        const assets::RoutePoint& point = route.points[i];
        position += point.offset;
        const Vector3 left = LeftOf(point.rotation);
        const Vector3 probe = position + Vector3{.z = kProbeLift};
        const float clearLeft = ClearWidth(world, probe, left, point.widthLeft);
        const float clearRight = ClearWidth(world, probe, left * -1.0F, point.widthRight);
        if (clearLeft < point.widthLeft || clearRight < point.widthRight) {
            ++walledPoints;
        }
        corridor.centres.push_back(position);
        corridor.lefts.push_back(left);
        corridor.lows.push_back(-std::max(0.0F, clearRight - kHalfCarWidth));
        corridor.highs.push_back(std::max(0.0F, clearLeft - kHalfCarWidth));
        corridor.isFixed.push_back(i == 0 || i + 1 == count || i == route.loopPoint);
    }
    return corridor;
}

Vector3 PointOf(const Corridor& corridor, std::span<const float> laterals, std::size_t i) {
    return corridor.centres[i] + (corridor.lefts[i] * laterals[i]);
}

void Smooth(const Corridor& corridor, std::vector<float>& laterals) {
    const std::size_t count = laterals.size();
    std::vector<Vector3> flat;
    flat.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        flat.push_back(Flat(PointOf(corridor, laterals, i)));
    }
    for (int pass = 0; pass < kSmoothingPasses; ++pass) {
        for (std::size_t i = 1; i + 1 < count; ++i) {
            if (corridor.isFixed[i]) {
                continue;
            }
            const Vector3& previous = flat[i - 1];
            const Vector3& current = flat[i];
            const float before = Length(current - previous);
            const float total = before + Length(flat[i + 1] - current);
            if (total <= 0.0F) {
                continue;
            }
            const Vector3 target = previous + ((flat[i + 1] - previous) * (before / total));
            const float lateral = std::clamp(laterals[i] + Dot(target - current, corridor.lefts[i]),
                                             corridor.lows[i], corridor.highs[i]);
            flat[i] = flat[i] + (corridor.lefts[i] * (lateral - laterals[i]));
            laterals[i] = lateral;
        }
    }
}

bool PullBackFromWalls(const CollisionQuery& world, Corridor& corridor,
                       std::span<const float> laterals) {
    bool isClear = true;
    const Vector3 lift{.z = kProbeLift};
    for (std::size_t i = 0; i + 1 < laterals.size(); ++i) {
        const Vector3 from = PointOf(corridor, laterals, i) + lift;
        if (CastSolid(world, from, PointOf(corridor, laterals, i + 1) + lift)) {
            for (const std::size_t end : {i, i + 1}) {
                corridor.lows[end] *= kWallPullBack;
                corridor.highs[end] *= kWallPullBack;
            }
            isClear = false;
        }
    }
    return isClear;
}

}

RacingLine RacingLine::Fit(const assets::RaceRoute& route, const CollisionQuery& world) {
    RacingLine line;
    if (route.points.empty()) {
        return line;
    }
    Corridor corridor = CorridorOf(route, world, line.walledPoints_);
    line.laterals_.assign(route.points.size(), 0.0F);
    for (int round = 0; round < kWallRounds; ++round) {
        Smooth(corridor, line.laterals_);
        if (PullBackFromWalls(world, corridor, line.laterals_)) {
            break;
        }
        for (std::size_t i = 0; i < line.laterals_.size(); ++i) {
            line.laterals_[i] = std::clamp(line.laterals_[i], corridor.lows[i], corridor.highs[i]);
        }
    }
    for (std::size_t i = 0; i < line.laterals_.size(); ++i) {
        line.points_.push_back(PointOf(corridor, line.laterals_, i));
    }
    const std::size_t last = route.points.size() - 1;
    line.loopStart_ = std::min(route.loopPoint + 1, last);
    line.isClosed_ =
        route.loopPoint < last &&
        Length(corridor.centres[last] - corridor.centres[route.loopPoint]) <= kLoopJoin;
    return line;
}

float RacingLine::LateralAt(std::size_t target, float alpha) const {
    if (target >= laterals_.size()) {
        return 0.0F;
    }
    const float from = target == 0 ? 0.0F : laterals_[target - 1];
    return from + ((laterals_[target] - from) * std::clamp(alpha, 0.0F, 1.0F));
}

std::size_t RacingLine::Next(std::size_t point) const {
    if (point + 1 < points_.size()) {
        return point + 1;
    }
    return isClosed_ ? loopStart_ : point;
}

}
