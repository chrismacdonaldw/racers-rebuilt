#include "assets/target_points.hpp"

#include <array>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kPointField = 0x27;

struct StoredPoint {
    std::optional<Vector3> position;
    std::optional<std::int32_t> removingEvent;
};

constexpr std::array kPointFields{
    Field<&StoredPoint::position>(0x28),
    Field<&StoredPoint::removingEvent>(0x29),
};

Result<TargetPoint> ReadPoint(TokenReader& reader) {
    StoredPoint stored;
    return ExpectField(reader, kPointField)
        .and_then([&] { return ReadBlock<StoredPoint>(reader, kPointFields, stored); })
        .and_then([&]() -> Result<TargetPoint> {
            if (!stored.position) {
                return Fail(ErrorCode::InvalidData, "a target point is missing its position");
            }
            return TargetPoint{.position = *stored.position, .removingEvent = stored.removingEvent};
        });
}

}

Result<std::vector<TargetPoint>> ParseTgb(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    std::vector<TargetPoint> points;
    return ExpectField(reader, kPointField)
        .and_then([&] {
            return ReadList(reader, [&] {
                return ReadPoint(reader).transform(
                    [&](const TargetPoint& point) { points.push_back(point); });
            });
        })
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .transform([&](const Token&) { return std::move(points); });
}

}
