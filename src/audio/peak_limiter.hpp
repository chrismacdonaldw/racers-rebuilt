#pragma once

#include <cstdint>
#include <span>

namespace rr::audio {

/// Scales the mix by `inputGain`, then turns it down at once whenever it would pass full scale.
class PeakLimiter {
public:
    PeakLimiter(std::uint32_t sampleRateHz, float inputGain);

    void Process(std::span<float> interleavedStereo) noexcept;

    [[nodiscard]] float Gain() const { return gain_; }

private:
    float release_;
    float inputGain_;
    float gain_ = 1.0F;
};

}
