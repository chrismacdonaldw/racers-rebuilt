#include "assets/route_markers.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string_view>
#include <utility>

namespace rr::assets {
namespace {

constexpr std::size_t kVec3 = 3;
constexpr std::size_t kVec4 = 4;

struct RouteLists {
    std::span<const JsonValue> durations;
    std::span<const JsonValue> rotations;
    std::span<const JsonValue> widths;
    std::span<const JsonValue> kinds;
};

const JsonValue* ItemAt(std::span<const JsonValue> items, std::size_t index) {
    return index < items.size() ? &items[index] : nullptr;
}

float FloatAt(std::span<const JsonValue> items, std::size_t index, float fallback) {
    const JsonValue* item = ItemAt(items, index);
    return item != nullptr ? item->Float().value_or(fallback) : fallback;
}

template <typename Whole>
Result<Whole> IntegerOf(const JsonValue* value, std::string_view key, Whole fallback) {
    if (value == nullptr) {
        return fallback;
    }
    const std::optional<Whole> number = value->Integer<Whole>();
    if (!number) {
        return Fail(ErrorCode::InvalidData, std::format("{} needs whole numbers in range", key));
    }
    return *number;
}

Quaternion RotationAt(std::span<const JsonValue> items, std::size_t first) {
    return {.w = FloatAt(items, first, 1.0F),
            .x = FloatAt(items, first + 1, 0.0F),
            .y = FloatAt(items, first + 2, 0.0F),
            .z = FloatAt(items, first + 3, 0.0F)};
}

std::optional<Quaternion> RotationOf(const JsonValue& extras, std::string_view key) {
    const std::span<const JsonValue> items = extras.Items(key);
    return items.size() == kVec4 ? std::optional(RotationAt(items, 0)) : std::nullopt;
}

std::optional<Vector3> PointOf(const JsonValue& extras, std::string_view key) {
    const std::span<const JsonValue> items = extras.Items(key);
    if (items.size() != kVec3) {
        return std::nullopt;
    }
    return Vector3{
        .x = FloatAt(items, 0, 0.0F), .y = FloatAt(items, 1, 0.0F), .z = FloatAt(items, 2, 0.0F)};
}

Quaternion Heading(const Vector3& from, const Vector3& to) {
    const float yaw = std::atan2(to.y - from.y, to.x - from.x);
    return {.w = std::cos(yaw / 2.0F), .z = std::sin(yaw / 2.0F)};
}

Result<RoutePoint> StepTo(std::span<const Vector3> line, std::size_t to, const RouteLists& lists) {
    const std::size_t at = to - 1;
    const bool hasRotations = lists.rotations.size() == (line.size() - 1) * kVec4;
    RoutePoint point{.offset = line[to] - line[at],
                     .rotation = hasRotations ? RotationAt(lists.rotations, at * kVec4)
                                              : Heading(line[at], line[to]),
                     .widthLeft = FloatAt(lists.widths, at * 2, 0.0F),
                     .widthRight = FloatAt(lists.widths, (at * 2) + 1, 0.0F)};
    return IntegerOf<std::uint8_t>(ItemAt(lists.kinds, at), "kinds", 0)
        .and_then([&](std::uint8_t kind) {
            point.kind = kind;
            return IntegerOf<std::int32_t>(ItemAt(lists.durations, at), "durations_ms", 0);
        })
        .transform([&](std::int32_t durationMs) {
            point.durationMs = durationMs;
            return point;
        });
}

Result<void> ReadLoop(RaceRoute& route, const JsonValue& extras) {
    return IntegerOf<std::size_t>(extras.Find("loop_point"), "loop_point", 0)
        .and_then([&](std::size_t loopPoint) {
            route.loopPoint = loopPoint;
            return IntegerOf<std::int32_t>(extras.Find("loop_time_ms"), "loop_time_ms",
                                           route.points.front().durationMs);
        })
        .and_then([&](std::int32_t loopTimeMs) {
            route.loopTimeMs = loopTimeMs;
            route.loopPosition = PointOf(extras, "loop_position")
                                     .value_or(route.startPosition + route.points.front().offset);
            route.loopRotation =
                RotationOf(extras, "loop_rotation").value_or(route.points.front().rotation);
            return ValidateRoute(route);
        });
}

}

Result<RaceRoute> RouteFromLine(std::span<const Vector3> line, const JsonValue& extras) {
    if (line.size() < 2) {
        return Fail(ErrorCode::InvalidData, "a route needs a line of two points or more");
    }
    const RouteLists lists{.durations = extras.Items("durations_ms"),
                           .rotations = extras.Items("rotations"),
                           .widths = extras.Items("widths"),
                           .kinds = extras.Items("kinds")};
    if (lists.durations.size() != line.size() - 1) {
        return Fail(ErrorCode::InvalidData,
                    std::format("durations_ms needs {} values", line.size() - 1));
    }
    RaceRoute route{
        .startPosition = line.front(),
        .startRotation = RotationOf(extras, "start_rotation").value_or(Heading(line[0], line[1]))};
    route.points.reserve(line.size() - 1);
    for (std::size_t to = 1; to < line.size(); ++to) {
        Result<RoutePoint> point = StepTo(line, to, lists);
        if (!point) {
            return std::unexpected(std::move(point.error()));
        }
        route.points.push_back(*point);
    }
    return ReadLoop(route, extras).transform([&] { return std::move(route); });
}

}
