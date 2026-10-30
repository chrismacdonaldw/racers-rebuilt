#include "assets/race_route.hpp"

#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <utility>

#include "assets/stored_rotation.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr float kOffsetXYScale = 1.0F / 256.0F;
constexpr float kOffsetZScale = 1.0F / 16.0F;
constexpr float kRotationScale = 1.0F / 127.0F;
constexpr float kWidthScale = 1.0F / 8.0F;
constexpr int kKindShift = 6;
constexpr std::int32_t kDurationMask = 0x3F;
constexpr int kDurationShift = 5;

using PackedPoint = std::array<std::int32_t, 10>;

struct StoredRoute {
    Vector3 startPosition;
    StoredRotation startRotation = kStoredIdentity;
    Vector3 loopPosition;
    StoredRotation loopRotation = kStoredIdentity;
    std::int32_t loopTimeMs = 0;
    std::int32_t loopPoint = 0;
    std::vector<PackedPoint> points;
};

constexpr std::array kRouteFields{
    Field<&StoredRoute::points>(0x27),        Field<&StoredRoute::startRotation>(0x28),
    Field<&StoredRoute::startPosition>(0x29), Field<&StoredRoute::loopPosition>(0x2A),
    Field<&StoredRoute::loopRotation>(0x2B),  Field<&StoredRoute::loopTimeMs>(0x2C),
    Field<&StoredRoute::loopPoint>(0x2D),
};

constexpr std::size_t kFirstByte = 2;

Result<void> CheckRanges(const PackedPoint& packed) {
    for (std::size_t i = 0; i < packed.size(); ++i) {
        const bool isOffset = i < kFirstByte;
        const std::int32_t low = isOffset ? std::numeric_limits<std::int16_t>::min() : 0;
        const std::int32_t high = isOffset ? std::numeric_limits<std::int16_t>::max()
                                           : std::numeric_limits<std::uint8_t>::max();
        if (packed.at(i) < low || packed.at(i) > high) {
            return Fail(ErrorCode::InvalidData,
                        std::format("route point value {} is out of range", packed.at(i)));
        }
    }
    if ((packed[9] & kDurationMask) == 0) {
        return Fail(ErrorCode::InvalidData, "route point has no duration");
    }
    return {};
}

float SignedByte(std::int32_t stored, float scale) {
    return static_cast<float>(static_cast<std::int8_t>(static_cast<std::uint8_t>(stored))) * scale;
}

Result<RoutePoint> DecodePoint(const PackedPoint& packed) {
    const auto rotation = [&](std::size_t i) { return SignedByte(packed.at(i), kRotationScale); };
    return CheckRanges(packed)
        .and_then([&] {
            return FromStoredRotation({rotation(3), rotation(4), rotation(5), rotation(6)});
        })
        .transform([&](const Quaternion& turned) {
            return RoutePoint{
                .offset = {.x = static_cast<float>(packed[0]) * kOffsetXYScale,
                           .y = static_cast<float>(packed[1]) * kOffsetXYScale,
                           .z = SignedByte(packed[2], kOffsetZScale)},
                .rotation = turned,
                .widthLeft = SignedByte(packed[7], kWidthScale),
                .widthRight = SignedByte(packed[8], kWidthScale),
                .kind = static_cast<std::uint8_t>(packed[9] >> kKindShift),
                .durationMs = (packed[9] & kDurationMask) << kDurationShift,
            };
        });
}

Result<RaceRoute> Decode(const StoredRoute& stored) {
    if (stored.loopPoint < 0 ||
        std::cmp_greater_equal(stored.loopPoint + 1, stored.points.size())) {
        return Fail(ErrorCode::InvalidData, std::format("route loops to point {} of {}",
                                                        stored.loopPoint, stored.points.size()));
    }
    RaceRoute route{
        .startPosition = stored.startPosition,
        .loopPosition = stored.loopPosition,
        .loopTimeMs = stored.loopTimeMs,
        .loopPoint = static_cast<std::size_t>(stored.loopPoint),
    };
    route.points.reserve(stored.points.size());
    for (const PackedPoint& packed : stored.points) {
        Result<RoutePoint> point = DecodePoint(packed);
        if (!point) {
            return std::unexpected(std::move(point.error()));
        }
        route.points.push_back(*point);
    }
    return FromStoredRotation(stored.startRotation)
        .and_then([&](const Quaternion& start) {
            route.startRotation = start;
            return FromStoredRotation(stored.loopRotation);
        })
        .transform([&](const Quaternion& loop) {
            route.loopRotation = loop;
            return std::move(route);
        });
}

}

Result<RaceRoute> ParseRaceRoute(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    StoredRoute stored;
    return ReadFields<StoredRoute>(reader, kRouteFields, stored)
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .and_then([&](const Token&) { return Decode(stored); });
}

}
