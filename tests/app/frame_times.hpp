#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

#include "core/duration.hpp"

namespace rr::test {

[[nodiscard]] inline std::vector<Nanoseconds> FixedRateFrames(int fps, Nanoseconds duration) {
    std::vector<Nanoseconds> frames;
    const std::int64_t frameCount =
        duration.count() * fps / Nanoseconds{std::chrono::seconds{1}}.count();
    for (std::int64_t i = 0; i <= frameCount; ++i) {
        frames.emplace_back(i * Nanoseconds{std::chrono::seconds{1}}.count() / fps);
    }
    return frames;
}

[[nodiscard]] inline std::vector<Nanoseconds> UncappedFrames(Nanoseconds duration) {
    constexpr std::int64_t kMinFrameNs = 200'000;
    constexpr std::int64_t kFrameSpreadNs = 2'800'000;
    constexpr std::int64_t kStride = 7'919;
    std::vector<Nanoseconds> frames{Nanoseconds{0}};
    for (std::int64_t i = 1; frames.back() < duration; ++i) {
        const Nanoseconds frameTime{kMinFrameNs + (i * kStride * 1'000 % kFrameSpreadNs)};
        frames.push_back(std::min(frames.back() + frameTime, duration));
    }
    return frames;
}

}
