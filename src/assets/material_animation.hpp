#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "core/duration.hpp"
#include "core/result.hpp"

namespace rr::assets {

/// Shows `material` from cycle frame `frame` until the next key of its track.
struct MaterialKey {
    std::string material;
    std::int32_t frame = 0;

    friend bool operator==(const MaterialKey&, const MaterialKey&) = default;
};

struct MaterialTrack {
    std::size_t firstKey = 0;
    std::size_t keyCount = 1;
    std::int32_t duration = 1;
    std::int32_t framesPerSecond = 30;

    friend bool operator==(const MaterialTrack&, const MaterialTrack&) = default;
};

struct MaterialAnimationAsset {
    std::vector<MaterialKey> keys;
    std::vector<MaterialTrack> tracks;
};

/// Fails on a track whose keys leave the list, don't ascend or fall outside its cycle.
[[nodiscard]] Result<MaterialAnimationAsset> ParseMaterialAnimation(
    std::span<const std::byte> bytes);

/// An index into `animation.keys`. Restarts each cycle, and shows the last key before the first.
[[nodiscard]] std::size_t SampleTrack(const MaterialAnimationAsset& animation, std::size_t track,
                                      Milliseconds age);

/// As `SampleTrack`, once `frames` of the track have played.
[[nodiscard]] std::size_t KeyAtFrame(const MaterialAnimationAsset& animation, std::size_t track,
                                     std::int64_t frames);

}
