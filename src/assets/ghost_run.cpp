#include "assets/ghost_run.hpp"

#include <algorithm>
#include <format>
#include <limits>
#include <numeric>
#include <utility>

#include "assets/stored_rotation.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kGhostField = 0x2C;
constexpr float kPositionScale = 32.0F;
constexpr float kRotationScale = 127.0F;
constexpr std::size_t kRotationStart = 3;

using StoredSample = std::array<std::int32_t, 7>;

struct StoredGhost {
    GhostLapTimes laps{};
    Vector3 startPosition;
    StoredRotation startRotation = kStoredIdentity;
    std::vector<StoredSample> samples;
};

constexpr std::array kGhostFields{
    Field<&StoredGhost::laps>(0x2A),          Field<&StoredGhost::startPosition>(0x28),
    Field<&StoredGhost::startRotation>(0x29), IgnoredField<StoredGhost, std::int32_t>(0x2B),
    Field<&StoredGhost::samples>(0x27),
};

template <typename Int>
Int Quantize(float value, float scale) {
    return static_cast<Int>(std::clamp(value * scale,
                                       static_cast<float>(std::numeric_limits<Int>::min()),
                                       static_cast<float>(std::numeric_limits<Int>::max())));
}

Result<GhostSample> DecodeSample(const StoredSample& stored) {
    GhostSample sample;
    for (std::size_t i = 0; i < sample.position.size(); ++i) {
        if (stored.at(i) < std::numeric_limits<std::int16_t>::min() ||
            stored.at(i) > std::numeric_limits<std::int16_t>::max()) {
            return Fail(ErrorCode::InvalidData,
                        std::format("ghost position {} is out of range", stored.at(i)));
        }
        sample.position.at(i) = static_cast<std::int16_t>(stored.at(i));
    }
    for (std::size_t i = 0; i < sample.rotation.size(); ++i) {
        const std::int32_t value = stored.at(kRotationStart + i);
        if (std::cmp_less(value, std::numeric_limits<std::int8_t>::min()) ||
            std::cmp_greater(value, std::numeric_limits<std::uint8_t>::max())) {
            return Fail(ErrorCode::InvalidData,
                        std::format("ghost rotation {} is out of range", value));
        }
        sample.rotation.at(i) = static_cast<std::int8_t>(value);
    }
    if (std::ranges::all_of(sample.rotation, [](std::int8_t part) { return part == 0; })) {
        return Fail(ErrorCode::InvalidData, "ghost sample has a zero rotation");
    }
    return sample;
}

Result<GhostRun> Decode(const StoredGhost& stored) {
    if (stored.samples.size() > kGhostSampleCapacity) {
        return Fail(ErrorCode::InvalidData,
                    std::format("ghost has {} samples, more than {}", stored.samples.size(),
                                kGhostSampleCapacity));
    }
    if (std::ranges::any_of(stored.laps, [](Milliseconds lap) { return lap.count() < 0; })) {
        return Fail(ErrorCode::InvalidData, "ghost has a negative lap time");
    }
    GhostRun run{.laps = stored.laps};
    run.samples.reserve(stored.samples.size());
    for (const StoredSample& packed : stored.samples) {
        Result<GhostSample> sample = DecodeSample(packed);
        if (!sample) {
            return std::unexpected(std::move(sample.error()));
        }
        run.samples.push_back(*sample);
    }
    return FromStoredRotation(stored.startRotation).transform([&](const Quaternion& rotation) {
        run.start = {.position = stored.startPosition, .rotation = rotation};
        return std::move(run);
    });
}

}

Milliseconds RaceTime(const GhostRun& run) {
    return std::accumulate(run.laps.begin(), run.laps.end(), Milliseconds{0});
}

GhostSample ToGhostSample(const Transform& pose) {
    const Quaternion& rotation = pose.rotation;
    const auto position = [&](float value) {
        return Quantize<std::int16_t>(value, kPositionScale);
    };
    const auto part = [](float value) { return Quantize<std::int8_t>(value, kRotationScale); };
    return {
        .position = {position(pose.position.x), position(pose.position.y),
                     position(pose.position.z)},
        .rotation = {part(-rotation.x), part(-rotation.y), part(-rotation.z), part(rotation.w)},
    };
}

Transform FromGhostSample(const GhostSample& sample) {
    const auto position = [&](std::size_t i) {
        return static_cast<float>(sample.position.at(i)) / kPositionScale;
    };
    const auto part = [&](std::size_t i) {
        return static_cast<float>(sample.rotation.at(i)) / kRotationScale;
    };
    return {.position = {.x = position(0), .y = position(1), .z = position(2)},
            .rotation =
                FromStoredRotation({part(0), part(1), part(2), part(3)}).value_or(Quaternion{})};
}

Result<GhostRun> ParseGhb(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    StoredGhost stored;
    return ExpectField(reader, kGhostField)
        .and_then([&] { return ReadBlock<StoredGhost>(reader, kGhostFields, stored); })
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .and_then([&](const Token&) { return Decode(stored); });
}

}
