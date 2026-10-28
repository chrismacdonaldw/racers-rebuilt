#pragma once

#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "core/vector3.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/surface.hpp"

namespace rr::test {

struct TestPlane {
    Vector3 normal{.z = 1.0F};
    float offset = 0.0F;
    sim::Surface surface;
    float minX = std::numeric_limits<float>::lowest();
    float maxX = std::numeric_limits<float>::max();
    bool isSolid = true;
};

class PlaneWorld final : public sim::CollisionQuery {
public:
    explicit PlaneWorld(std::vector<TestPlane> planes) : planes_(std::move(planes)) {}

    [[nodiscard]] std::optional<sim::SegmentHit> CastSegment(const Vector3& from,
                                                             const Vector3& to) const override {
        std::optional<sim::SegmentHit> nearest;
        for (const TestPlane& plane : planes_) {
            const float start = Dot(plane.normal, from) - plane.offset;
            const float end = Dot(plane.normal, to) - plane.offset;
            if (start < 0.0F || end >= 0.0F) {
                continue;
            }
            const float fraction = start / (start - end);
            const Vector3 point = from + ((to - from) * fraction);
            const bool isInside = point.x >= plane.minX && point.x <= plane.maxX;
            if (isInside && (!nearest || fraction < nearest->fraction)) {
                nearest = sim::SegmentHit{.point = point,
                                          .normal = plane.normal,
                                          .fraction = fraction,
                                          .surface = plane.surface,
                                          .isSolid = plane.isSolid};
            }
        }
        return nearest;
    }

private:
    std::vector<TestPlane> planes_;
};

[[nodiscard]] inline PlaneWorld FlatGround() {
    return PlaneWorld({TestPlane{}});
}

}
