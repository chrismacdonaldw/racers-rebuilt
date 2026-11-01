#include "assets/material_animation.hpp"

#include <array>
#include <format>
#include <optional>
#include <ranges>
#include <utility>

#include "assets/token_schema.hpp"

namespace rr::assets {
namespace {

constexpr FieldId kKeysField = 0x27;
constexpr FieldId kTracksField = 0x28;
constexpr std::size_t kMaxNameLength = 8;
constexpr float kMillisecondsPerSecond = 1000.0F;

struct StoredTrack {
    std::int32_t firstKey = 0;
    std::int32_t keyCount = 1;
    std::int32_t duration = 1;
    std::int32_t framesPerSecond = 30;
};

constexpr std::array kTrackFields{
    Field<&StoredTrack::firstKey, &StoredTrack::keyCount>(0x27),
    Field<&StoredTrack::duration>(0x29),
    Field<&StoredTrack::framesPerSecond>(0x2A),
};

Result<void> FailIfEmpty(std::size_t count, std::string_view what) {
    if (count == 0) {
        return Fail(ErrorCode::InvalidData, std::format("a material animation has no {}", what));
    }
    return {};
}

Result<void> ReadKeys(TokenReader& reader, std::vector<MaterialKey>& keys) {
    return ExpectField(reader, kKeysField)
        .and_then([&] {
            return ReadList(reader, [&] {
                MaterialKey& key = keys.emplace_back();
                return ReadValue(reader, key.material).and_then([&] {
                    return ReadValue(reader, key.frame);
                });
            });
        })
        .and_then([&] { return FailIfEmpty(keys.size(), "keys"); });
}

std::optional<std::string> Problem(const StoredTrack& track, std::span<const MaterialKey> keys) {
    if (track.firstKey < 0 || track.keyCount < 1 ||
        static_cast<std::size_t>(track.firstKey) + static_cast<std::size_t>(track.keyCount) >
            keys.size()) {
        return "its keys leave the key list";
    }
    if (track.duration < 1 || track.framesPerSecond < 1) {
        return "its cycle or speed is not positive";
    }
    const std::span<const MaterialKey> own = keys.subspan(static_cast<std::size_t>(track.firstKey),
                                                          static_cast<std::size_t>(track.keyCount));
    const bool ascends = std::ranges::is_sorted(own, {}, &MaterialKey::frame);
    if (!ascends || own.front().frame < 0 || own.back().frame >= track.duration) {
        return "its keys don't ascend within its cycle";
    }
    return std::nullopt;
}

Result<void> ReadTrack(TokenReader& reader, std::span<const MaterialKey> keys,
                       std::vector<MaterialTrack>& tracks) {
    StoredTrack stored;
    return ExpectField(reader, kTracksField)
        .and_then([&] { return ReadBlock<StoredTrack>(reader, kTrackFields, stored); })
        .and_then([&]() -> Result<void> {
            if (const std::optional<std::string> problem = Problem(stored, keys)) {
                return Fail(ErrorCode::InvalidData, std::format("material track {} is invalid: {}",
                                                                tracks.size(), *problem));
            }
            tracks.push_back({.firstKey = static_cast<std::size_t>(stored.firstKey),
                              .keyCount = static_cast<std::size_t>(stored.keyCount),
                              .duration = stored.duration,
                              .framesPerSecond = stored.framesPerSecond});
            return {};
        });
}

Result<void> CheckNames(std::span<const MaterialKey> keys) {
    if (std::ranges::any_of(
            keys, [](const MaterialKey& key) { return key.material.size() > kMaxNameLength; })) {
        return Fail(ErrorCode::InvalidData, "a material name is longer than 8 characters");
    }
    return {};
}

}

Result<MaterialAnimationAsset> ParseMaterialAnimation(std::span<const std::byte> bytes) {
    TokenReader reader(bytes);
    MaterialAnimationAsset animation;
    return ReadKeys(reader, animation.keys)
        .and_then([&] { return CheckNames(animation.keys); })
        .and_then([&] { return ExpectField(reader, kTracksField); })
        .and_then([&] {
            return ReadList(reader,
                            [&] { return ReadTrack(reader, animation.keys, animation.tracks); });
        })
        .and_then([&] { return FailIfEmpty(animation.tracks.size(), "tracks"); })
        .and_then([&] { return reader.Expect(TokenKind::End); })
        .transform([&](const Token&) { return std::move(animation); });
}

std::size_t SampleTrack(const MaterialAnimationAsset& animation, std::size_t track,
                        Milliseconds age) {
    const MaterialTrack& played = animation.tracks[track];
    const auto elapsedFrames =
        static_cast<std::int64_t>(static_cast<float>(played.framesPerSecond) /
                                  kMillisecondsPerSecond * static_cast<float>(age.count()));
    const std::int64_t frame = elapsedFrames % played.duration;
    const std::span<const MaterialKey> keys =
        std::span(animation.keys).subspan(played.firstKey, played.keyCount);
    std::size_t shown = played.keyCount - 1;
    for (std::size_t i = 0; i < keys.size() && keys[i].frame <= frame; ++i) {
        shown = i;
    }
    return played.firstKey + shown;
}

}
