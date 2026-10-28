#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "audio/pcm_stream.hpp"

namespace rr::audio {

/// In frames. Without an end, the loop runs to the end of the stream.
struct LoopPoints {
    std::uint64_t startFrame = 0;
    std::optional<std::uint64_t> endFrame;
};

/// Never ends: after the loop end it continues from the loop start.
class LoopingStream final : public PcmStream {
public:
    LoopingStream(std::unique_ptr<PcmStream> stream, LoopPoints loop);

    [[nodiscard]] PcmFormat Format() const override { return stream_->Format(); }
    [[nodiscard]] std::uint64_t FrameCount() const override { return stream_->FrameCount(); }
    [[nodiscard]] std::uint64_t Cursor() const override { return stream_->Cursor(); }

    std::uint64_t Read(std::span<std::int16_t> interleaved) noexcept override;
    void Seek(std::uint64_t frame) noexcept override { stream_->Seek(frame); }

private:
    std::unique_ptr<PcmStream> stream_;
    LoopPoints loop_;
};

}
