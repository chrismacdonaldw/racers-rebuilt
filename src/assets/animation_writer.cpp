#include "assets/animation_writer.hpp"

#include <utility>

#include "assets/token_writer.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kPoolsField = 0x27;
constexpr FieldId kPositionKeysField = 0x28;
constexpr FieldId kRotationKeysField = 0x29;
constexpr FieldId kKeyFramesField = 0x2A;
constexpr FieldId kTracksField = 0x2B;
constexpr FieldId kPartsField = 0x2C;
constexpr FieldId kFirstTrackField = 0x2B;
constexpr FieldId kFrameCountField = 0x2D;
constexpr FieldId kLoopFramesField = 0x2E;
constexpr FieldId kFramesPerSecondField = 0x2F;
constexpr FieldId kVelocityField = 0x30;
constexpr FieldId kBoundsField = 0x31;

void Floats(TokenWriter& writer, std::initializer_list<float> values) {
    for (const float value : values) {
        writer.Float(value);
    }
}

void WritePools(TokenWriter& writer, const AnimationAsset& animations) {
    writer.Field(kPoolsField);
    writer.OpenBlock();
    writer.Field(kPositionKeysField);
    writer.OpenList(std::max<std::size_t>(animations.positionKeys.size(), 1));
    for (const Vector3& key : animations.positionKeys) {
        Floats(writer, {key.x, key.y, key.z});
    }
    if (animations.positionKeys.empty()) {
        Floats(writer, {0.0F, 0.0F, 0.0F});
    }
    writer.Close();
    writer.Field(kRotationKeysField);
    writer.OpenList(std::max<std::size_t>(animations.rotationKeys.size(), 1));
    for (const Quaternion& key : animations.rotationKeys) {
        Floats(writer, {-key.x, -key.y, -key.z, key.w});
    }
    if (animations.rotationKeys.empty()) {
        Floats(writer, {0.0F, 0.0F, 0.0F, 1.0F});
    }
    writer.Close();
    writer.Field(kKeyFramesField);
    writer.OpenList(std::max<std::size_t>(animations.keyFrames.size(), 1));
    for (const std::uint16_t frame : animations.keyFrames) {
        writer.Int(frame);
    }
    if (animations.keyFrames.empty()) {
        writer.Int(0);
    }
    writer.Close();
    writer.Close();
}

void WriteTracks(TokenWriter& writer, const std::vector<AnimationTrack>& tracks) {
    writer.Field(kTracksField);
    writer.OpenList(tracks.size());
    for (const AnimationTrack& track : tracks) {
        for (const KeyRun& run : {track.rotations, track.positions}) {
            writer.Int(static_cast<std::int32_t>(run.firstValue));
            writer.Int(static_cast<std::int32_t>(run.firstFrame));
            writer.Int(static_cast<std::int32_t>(run.count));
        }
    }
    writer.Close();
}

Result<void> WritePart(TokenWriter& writer, const AnimationPart& part) {
    writer.Field(kPartsField);
    return writer.String(part.name).transform([&] {
        writer.OpenBlock();
        writer.Field(kFirstTrackField);
        writer.Int(static_cast<std::int32_t>(part.firstTrack));
        writer.Field(kFrameCountField);
        writer.Int(part.frameCount);
        writer.Field(kLoopFramesField);
        writer.Int(part.loopFrames);
        writer.Field(kFramesPerSecondField);
        writer.Int(part.framesPerSecond);
        writer.Field(kVelocityField);
        Floats(writer, {part.velocity.x, part.velocity.y, part.velocity.z});
        writer.Field(kBoundsField);
        const BoundingSphere& bounds = part.bounds;
        Floats(writer, {bounds.centre.x, bounds.centre.y, bounds.centre.z, bounds.radius});
        writer.Close();
    });
}

}

Result<std::vector<std::byte>> WriteAnimations(const AnimationAsset& animations) {
    if (animations.tracks.empty() || animations.parts.empty()) {
        return Fail(ErrorCode::InvalidData, "animations need at least one track and one part");
    }
    TokenWriter writer;
    WritePools(writer, animations);
    WriteTracks(writer, animations.tracks);
    writer.Field(kPartsField);
    writer.OpenList(animations.parts.size());
    for (const AnimationPart& part : animations.parts) {
        if (Result<void> written = WritePart(writer, part); !written) {
            return std::unexpected(std::move(written.error()));
        }
    }
    writer.Close();
    return std::move(writer).Take();
}

}
