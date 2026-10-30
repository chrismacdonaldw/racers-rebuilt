#include "assets/animation_database.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>
#include <utility>

#include "assets/stored_rotation.hpp"
#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kPartField = 0x2C;

using StoredTrack = std::array<std::int32_t, 6>;

struct StoredPart {
    std::string name;
    std::int32_t firstTrack = 0;
    std::int32_t frameCount = 0;
    std::int32_t loopFrames = 0;
    std::int32_t framesPerSecond = 0;
    Vector3 velocity;
    Vector3 centre;
    float radius = 0.0F;
};

struct StoredAnimations {
    std::vector<Vector3> positionKeys;
    std::vector<StoredRotation> rotationKeys;
    std::vector<std::int32_t> keyFrames;
    std::vector<StoredTrack> tracks;
    std::vector<StoredPart> parts;
};

constexpr std::array kPartFields{
    Field<&StoredPart::firstTrack>(0x2B), Field<&StoredPart::frameCount>(0x2D),
    Field<&StoredPart::loopFrames>(0x2E), Field<&StoredPart::framesPerSecond>(0x2F),
    Field<&StoredPart::velocity>(0x30),   Field<&StoredPart::centre, &StoredPart::radius>(0x31),
};

constexpr std::array kPoolFields{
    Field<&StoredAnimations::positionKeys>(0x28),
    Field<&StoredAnimations::rotationKeys>(0x29),
    Field<&StoredAnimations::keyFrames>(0x2A),
};

Result<void> ReadPools(TokenReader& reader, StoredAnimations& stored) {
    return ReadBlock<StoredAnimations>(reader, kPoolFields, stored);
}

Result<void> ReadParts(TokenReader& reader, StoredAnimations& stored) {
    stored.parts.clear();
    return ReadList(reader, [&] {
        StoredPart& part = stored.parts.emplace_back();
        return ExpectField(reader, kPartField)
            .and_then([&] { return reader.ReadString(); })
            .and_then([&](std::string_view name) {
                part.name = name;
                return ReadBlock<StoredPart>(reader, kPartFields, part);
            });
    });
}

constexpr std::array kTopFields{
    FieldRule<StoredAnimations>{.id = 0x27, .read = ReadPools},
    Field<&StoredAnimations::tracks>(0x2B),
    FieldRule<StoredAnimations>{.id = kPartField, .read = ReadParts},
};

template <typename Stored, typename Decoded, typename DecodeOne>
Result<void> DecodeAll(const std::vector<Stored>& stored, std::vector<Decoded>& out,
                       DecodeOne decode) {
    out.reserve(stored.size());
    for (const Stored& value : stored) {
        Result<Decoded> decoded = decode(value);
        if (!decoded) {
            return std::unexpected(std::move(decoded.error()));
        }
        out.push_back(std::move(*decoded));
    }
    return {};
}

Result<std::uint16_t> DecodeFrame(std::int32_t frame) {
    if (!std::in_range<std::uint16_t>(frame)) {
        return Fail(ErrorCode::InvalidData, std::format("key frame {} is out of range", frame));
    }
    return static_cast<std::uint16_t>(frame);
}

bool IsWithin(std::size_t first, std::size_t count, std::size_t size) {
    return first <= size && count <= size - first;
}

bool IsValidRun(const KeyRun& run, std::size_t poolSize, std::span<const std::uint16_t> frames) {
    if (run.count <= 1) {
        return run.count == 0 || run.firstValue < poolSize;
    }
    return IsWithin(run.firstValue, run.count, poolSize) &&
           IsWithin(run.firstFrame, run.count, frames.size()) &&
           std::ranges::is_sorted(frames.subspan(run.firstFrame, run.count));
}

Result<KeyRun> DecodeRun(std::int32_t firstValue, std::int32_t firstFrame, std::int32_t count) {
    if (firstValue < 0 || firstFrame < 0 || count < 0) {
        return Fail(ErrorCode::InvalidData, std::format("track has a negative key run {} {} {}",
                                                        firstValue, firstFrame, count));
    }
    return KeyRun{.firstValue = static_cast<std::uint32_t>(firstValue),
                  .firstFrame = static_cast<std::uint32_t>(firstFrame),
                  .count = static_cast<std::uint32_t>(count)};
}

Result<AnimationTrack> DecodeTrack(const StoredTrack& stored, const AnimationAsset& pools) {
    Result<KeyRun> rotations = DecodeRun(stored[0], stored[1], stored[2]);
    Result<KeyRun> positions = rotations.and_then(
        [&](const KeyRun&) { return DecodeRun(stored[3], stored[4], stored[5]); });
    if (!positions) {
        return std::unexpected(std::move(positions.error()));
    }
    if (!IsValidRun(*rotations, pools.rotationKeys.size(), pools.keyFrames) ||
        !IsValidRun(*positions, pools.positionKeys.size(), pools.keyFrames)) {
        return Fail(ErrorCode::InvalidData, "track keys run past their pools or out of order");
    }
    return AnimationTrack{.rotations = *rotations, .positions = *positions};
}

Result<AnimationPart> DecodePart(const StoredPart& stored, std::size_t trackCount) {
    if (stored.firstTrack < 0 || std::cmp_greater_equal(stored.firstTrack, trackCount)) {
        return Fail(ErrorCode::InvalidData,
                    std::format("part {} starts at track {} of {}", stored.name, stored.firstTrack,
                                trackCount));
    }
    if (stored.frameCount < 0 || stored.loopFrames < 1 || stored.framesPerSecond < 1) {
        return Fail(ErrorCode::InvalidData,
                    std::format("part {} needs a positive loop and rate", stored.name));
    }
    return AnimationPart{.name = stored.name,
                         .firstTrack = static_cast<std::uint32_t>(stored.firstTrack),
                         .frameCount = stored.frameCount,
                         .loopFrames = stored.loopFrames,
                         .framesPerSecond = stored.framesPerSecond,
                         .velocity = stored.velocity,
                         .bounds = {.centre = stored.centre, .radius = stored.radius}};
}

Result<AnimationAsset> Decode(const StoredAnimations& stored) {
    if (stored.tracks.empty() || stored.parts.empty()) {
        return Fail(ErrorCode::InvalidData, "animations need at least one track and one part");
    }
    AnimationAsset animations{.positionKeys = stored.positionKeys};
    return DecodeAll(stored.rotationKeys, animations.rotationKeys, FromStoredRotation)
        .and_then([&] { return DecodeAll(stored.keyFrames, animations.keyFrames, DecodeFrame); })
        .and_then([&] {
            return DecodeAll(stored.tracks, animations.tracks, [&](const StoredTrack& track) {
                return DecodeTrack(track, animations);
            });
        })
        .and_then([&] {
            return DecodeAll(stored.parts, animations.parts, [&](const StoredPart& part) {
                return DecodePart(part, stored.tracks.size());
            });
        })
        .transform([&] { return std::move(animations); });
}

}

Result<AnimationAsset> ParseAnimations(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    StoredAnimations stored;
    return ReadFields<StoredAnimations>(reader, kTopFields, stored)
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .and_then([&](const Token&) { return Decode(stored); });
}

}
