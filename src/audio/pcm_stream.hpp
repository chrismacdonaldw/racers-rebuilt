#pragma once

#include <cstdint>
#include <span>

namespace rr::audio {

struct PcmFormat {
    std::uint32_t channelCount = 0;
    std::uint32_t sampleRateHz = 0;
};

/// Samples are interleaved 16-bit. `Read` and `Seek` run on the audio thread, so they must not
/// allocate, lock or block.
class PcmStream {
public:
    virtual ~PcmStream() = default;

    [[nodiscard]] virtual PcmFormat Format() const = 0;
    [[nodiscard]] virtual std::uint64_t FrameCount() const = 0;
    [[nodiscard]] virtual std::uint64_t Cursor() const = 0;

    /// Fewer frames than requested means the end was reached.
    virtual std::uint64_t Read(std::span<std::int16_t> interleaved) noexcept = 0;
    virtual void Seek(std::uint64_t frame) noexcept = 0;
};

}
