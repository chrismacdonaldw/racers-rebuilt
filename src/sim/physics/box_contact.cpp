#include "sim/physics/box_contact.hpp"

#include <array>
#include <cmath>
#include <cstddef>

namespace rr::sim {
namespace {

constexpr float kDegenerateRadius = 0.001F;
constexpr std::size_t kCornerCount = 8;

struct FaceAxis {
    Vector3 axis;
    bool isFirst = true;
    float halfSize = 0.0F;
};

std::array<Vector3, 3> AxesOf(const ContactBox& box) {
    return {box.axes.forward, box.axes.side, box.axes.up};
}

std::array<float, 3> HalvesOf(const ContactBox& box) {
    return {box.halfSize.x, box.halfSize.y, box.halfSize.z};
}

float ProjectedRadius(const ContactBox& box, const Vector3& axis) {
    const std::array<Vector3, 3> axes = AxesOf(box);
    const std::array<float, 3> halves = HalvesOf(box);
    float radius = 0.0F;
    for (std::size_t i = 0; i < axes.size(); ++i) {
        radius += halves.at(i) * std::abs(Dot(axes.at(i), axis));
    }
    return radius;
}

std::array<FaceAxis, 6> FaceAxesInOrder(const ContactBox& first, const ContactBox& second) {
    const std::array<Vector3, 3> a = AxesOf(first);
    const std::array<Vector3, 3> b = AxesOf(second);
    const std::array<float, 3> ha = HalvesOf(first);
    const std::array<float, 3> hb = HalvesOf(second);
    return {{{.axis = a[0], .isFirst = true, .halfSize = ha[0]},
             {.axis = b[0], .isFirst = false, .halfSize = hb[0]},
             {.axis = a[1], .isFirst = true, .halfSize = ha[1]},
             {.axis = a[2], .isFirst = true, .halfSize = ha[2]},
             {.axis = b[1], .isFirst = false, .halfSize = hb[1]},
             {.axis = b[2], .isFirst = false, .halfSize = hb[2]}}};
}

bool IsSeparatedOnEdgeAxes(const ContactBox& first, const ContactBox& second,
                           const Vector3& offset) {
    for (const Vector3& a : AxesOf(first)) {
        for (const Vector3& b : AxesOf(second)) {
            const Vector3 axis = Cross(a, b);
            const float radii = ProjectedRadius(first, axis) + ProjectedRadius(second, axis);
            if (std::abs(radii) > kDegenerateRadius && std::abs(Dot(offset, axis)) > radii) {
                return true;
            }
        }
    }
    return false;
}

void AddCorners(const ContactBox& box, const Vector3& outward, float threshold, Vector3& sum,
                int& count) {
    const std::array<Vector3, 3> axes = AxesOf(box);
    const std::array<float, 3> halves = HalvesOf(box);
    for (std::size_t corner = 0; corner < kCornerCount; ++corner) {
        Vector3 offset;
        for (std::size_t i = 0; i < axes.size(); ++i) {
            const float sign = ((corner >> i) & 1U) != 0 ? 1.0F : -1.0F;
            offset += axes.at(i) * (sign * halves.at(i));
        }
        if (Dot(offset, outward) >= threshold) {
            sum += box.centre + offset;
            ++count;
        }
    }
}

Vector3 ContactPoint(const ContactBox& first, const ContactBox& second, const FaceAxis& face,
                     const Vector3& normal, float penetration) {
    const float otherRadius =
        face.isFirst ? ProjectedRadius(second, normal) : ProjectedRadius(first, normal);
    Vector3 sum;
    int count = 0;
    AddCorners(first, normal, face.halfSize - penetration, sum, count);
    AddCorners(second, -normal, otherRadius - penetration, sum, count);
    if (count == 0) {
        return (first.centre + second.centre) * 0.5F;
    }
    return sum * (1.0F / static_cast<float>(count));
}

}

std::optional<BoxContact> TestBoxes(const ContactBox& first, const ContactBox& second) {
    const Vector3 offset = first.centre - second.centre;
    const std::array<FaceAxis, 6> faces = FaceAxesInOrder(first, second);
    const FaceAxis* chosen = nullptr;
    float penetration = 0.0F;
    for (const FaceAxis& face : faces) {
        const float overlap = ProjectedRadius(first, face.axis) +
                              ProjectedRadius(second, face.axis) - std::abs(Dot(offset, face.axis));
        if (overlap < 0.0F) {
            return std::nullopt;
        }
        if (chosen == nullptr || overlap < penetration) {
            chosen = &face;
            penetration = overlap;
        }
    }
    if (IsSeparatedOnEdgeAxes(first, second, offset) || penetration <= 0.0F) {
        return std::nullopt;
    }
    const Vector3 normal = Dot(offset, chosen->axis) < 0.0F ? -chosen->axis : chosen->axis;
    return BoxContact{.normal = normal,
                      .penetration = penetration,
                      .point = ContactPoint(first, second, *chosen, normal, penetration)};
}

}
