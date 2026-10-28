#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "audio/pcm_stream.hpp"
#include "core/result.hpp"

namespace rr::audio {

/// `data` borrows from the parsed bytes.
struct AlpSound {
    PcmFormat format;
    std::span<const std::byte> data;
};

[[nodiscard]] Result<AlpSound> ParseAlp(std::span<const std::byte> bytes);

/// Seeking backwards restarts from the first frame.
class AlpDecoder final : public PcmStream {
public:
    explicit AlpDecoder(AlpSound sound);

    [[nodiscard]] PcmFormat Format() const override { return sound_.format; }
    [[nodiscard]] std::uint64_t FrameCount() const override;
    [[nodiscard]] std::uint64_t Cursor() const override { return cursor_; }

    std::uint64_t Read(std::span<std::int16_t> interleaved) noexcept override;
    void Seek(std::uint64_t frame) noexcept override;

private:
    struct Channel {
        std::int32_t sample = 0;
        std::int32_t stepIndex = 0;

        std::int16_t Expand(std::uint8_t nibble) noexcept;
    };

    void Rewind() noexcept;

    AlpSound sound_;
    std::array<Channel, 2> channels_{};
    std::uint64_t cursor_ = 0;
};

}
