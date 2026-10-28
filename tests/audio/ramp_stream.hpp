#pragma once

#include <algorithm>
#include <cstdint>
#include <span>

#include "audio/pcm_stream.hpp"

namespace rr::test {

/// Mono frames whose sample value is their frame index.
class RampStream final : public audio::PcmStream {
public:
    explicit RampStream(std::uint64_t frameCount) : frameCount_(frameCount) {}

    [[nodiscard]] audio::PcmFormat Format() const override {
        return {.channelCount = 1, .sampleRateHz = 22050};
    }
    [[nodiscard]] std::uint64_t FrameCount() const override { return frameCount_; }
    [[nodiscard]] std::uint64_t Cursor() const override { return cursor_; }

    std::uint64_t Read(std::span<std::int16_t> interleaved) noexcept override {
        const std::uint64_t frames =
            std::min<std::uint64_t>(interleaved.size(), frameCount_ - cursor_);
        for (std::uint64_t frame = 0; frame < frames; ++frame) {
            interleaved[static_cast<std::size_t>(frame)] = static_cast<std::int16_t>(cursor_++);
        }
        return frames;
    }
    void Seek(std::uint64_t frame) noexcept override { cursor_ = std::min(frame, frameCount_); }

private:
    std::uint64_t frameCount_;
    std::uint64_t cursor_ = 0;
};

}
