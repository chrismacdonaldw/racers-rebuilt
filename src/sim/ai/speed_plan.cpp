#include "sim/ai/speed_plan.hpp"

#include <algorithm>
#include <cmath>

#include "sim/physics/drive_controller.hpp"
#include "sim/physics/ground_contact.hpp"

namespace rr::sim {
namespace {

constexpr float kCrawlCornerSpeed = 20.0F;
constexpr std::size_t kCornerSpan = 2;

Vector3 Flat(const Vector3& v) {
    return {.x = v.x, .y = v.y};
}

float Circumradius(const Vector3& a, const Vector3& b, const Vector3& c) {
    const Vector3 ab = Flat(b - a);
    const Vector3 bc = Flat(c - b);
    const Vector3 ca = Flat(a - c);
    const float twiceArea = std::abs((ab.x * bc.y) - (ab.y * bc.x));
    if (twiceArea <= 0.0F) {
        return 0.0F;
    }
    return Length(ab) * Length(bc) * Length(ca) / (2.0F * twiceArea);
}

std::size_t Previous(const RacingLine& line, std::size_t point) {
    if (line.IsClosed() && point == line.LoopStart()) {
        return line.Points().size() - 1;
    }
    return point == 0 ? 0 : point - 1;
}

std::size_t Step(const RacingLine& line, std::size_t point, bool isForward) {
    std::size_t at = point;
    for (std::size_t i = 0; i < kCornerSpan; ++i) {
        at = isForward ? line.Next(at) : Previous(line, at);
    }
    return at;
}

struct Corner {
    float speed = 0.0F;
    bool isDrift = false;
};

Corner CornerAt(const RacingLine& line, const CarLimits& car, std::size_t point) {
    const std::span<const Vector3> points = line.Points();
    const float radius = Circumradius(points[Step(line, point, false)], points[point],
                                      points[Step(line, point, true)]);
    if (radius <= 0.0F) {
        return {.speed = car.topSpeed};
    }
    const float grip =
        std::sqrt(std::max(0.0F, ((2.0F * radius) - car.lockRadius) * car.cornerGrip));
    const float drift =
        std::sqrt(std::max(0.0F, ((4.0F * radius) - (3.0F * car.lockRadius)) * car.cornerGrip));
    const float best = std::min(std::max({grip, drift, kCrawlCornerSpeed}), car.topSpeed);
    return {.speed = best, .isDrift = drift > grip && grip < car.topSpeed};
}

float BrakeFrom(const CarLimits& car, float speed, float distance) {
    const float share = speed / car.topSpeed;
    const float decel = car.brake * (1.0F + (share * share));
    return std::sqrt((speed * speed) + (2.0F * decel * distance));
}

float AccelerateFrom(const CarLimits& car, float speed, float distance) {
    const float share = std::min(speed / car.topSpeed, 1.0F);
    const float accel = car.thrust * (1.0F - (share * share));
    return std::sqrt((speed * speed) + (2.0F * accel * distance));
}

float Gap(const RacingLine& line, std::size_t from, std::size_t to) {
    return Length(Flat(line.Points()[to] - line.Points()[from]));
}

float SecondsPerLap(const RacingLine& line, std::span<const float> speeds) {
    float seconds = 0.0F;
    const std::size_t count = line.Points().size();
    for (std::size_t from = line.LoopStart(); from < count; ++from) {
        const std::size_t to = line.Next(from);
        const float average = (speeds[from] + speeds[to]) / 2.0F;
        seconds += average > 0.0F ? Gap(line, from, to) / average : 0.0F;
    }
    return seconds;
}

std::vector<std::size_t> LapOrder(const RacingLine& line) {
    std::vector<std::size_t> order;
    const std::size_t count = line.Points().size();
    order.reserve(count + (line.IsClosed() ? count - line.LoopStart() : 0));
    for (std::size_t i = 0; i < count; ++i) {
        order.push_back(i);
    }
    for (std::size_t i = line.LoopStart(); line.IsClosed() && i < count; ++i) {
        order.push_back(i);
    }
    return order;
}

}

CarLimits CarLimits::Of(const Racer& racer) {
    const BodyState& body = racer.Body();
    const float acceleration = racer.Stats().acceleration;
    return {
        .topSpeed = body.maxSpeed,
        .thrust = kPedalThrust * acceleration,
        .brake = kPedalThrust * kBrakeThrustScale * acceleration,
        .cornerGrip = AverageContactGrip(body.motion) * body.gripScale * racer.Constants().gravity,
        .lockRadius = std::abs(TurnRadiusFor(racer.Stats().handling))};
}

SpeedPlan::SpeedPlan(const RacingLine& line, const CarLimits& car) {
    const std::size_t count = line.Points().size();
    speeds_.resize(count);
    drifts_.resize(count);
    if (count == 0 || car.topSpeed <= 0.0F) {
        return;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const Corner corner = CornerAt(line, car, i);
        speeds_[i] = corner.speed;
        drifts_[i] = corner.isDrift;
    }
    const std::vector<std::size_t> order = LapOrder(line);
    for (std::size_t i = order.size() - 1; i > 0; --i) {
        const std::size_t from = order[i - 1];
        const std::size_t to = order[i];
        speeds_[from] = std::min(speeds_[from], BrakeFrom(car, speeds_[to], Gap(line, from, to)));
    }
    reachable_.resize(count);
    float speed = 0.0F;
    for (std::size_t i = 1; i < order.size(); ++i) {
        speed = std::min(speeds_[order[i]],
                         AccelerateFrom(car, speed, Gap(line, order[i - 1], order[i])));
        reachable_[order[i]] = speed;
    }
    lapSeconds_ = SecondsPerLap(line, reachable_);
}

float SpeedPlan::SpeedAt(std::size_t target, float alpha) const {
    if (target >= speeds_.size()) {
        return 0.0F;
    }
    const float from = target == 0 ? speeds_[0] : speeds_[target - 1];
    return from + ((speeds_[target] - from) * std::clamp(alpha, 0.0F, 1.0F));
}

}
