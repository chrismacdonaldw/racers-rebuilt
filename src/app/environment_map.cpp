#include "app/environment_map.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rr::app {
namespace {

constexpr int kLastStep = 1023;
constexpr double kHalfSteps = 511.5;

double TurnsOf(double cosine) {
    const int step = std::clamp(static_cast<int>((cosine + 1.0) * kHalfSteps), 0, kLastStep);
    return std::acos((static_cast<double>(step) / kHalfSteps) - 1.0) / std::numbers::pi;
}

}

render::UvOffset EnvironmentOffset(const Vector3& right, const Vector3& up) {
    if (right.x == 0.0F && right.y == 0.0F) {
        return {.v = 0.5F};
    }
    const double x = right.x;
    const double y = right.y;
    const double u = TurnsOf(x / std::hypot(x, y)) * 0.5;
    return {.u = static_cast<float>(y < 0.0 ? 1.0 - u : u), .v = static_cast<float>(TurnsOf(up.z))};
}

}
