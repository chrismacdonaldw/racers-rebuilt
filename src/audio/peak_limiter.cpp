#include "audio/peak_limiter.hpp"

#include <algorithm>
#include <cmath>

namespace rr::audio {
namespace {

constexpr float kReleaseSeconds = 0.2F;
constexpr std::size_t kChannelCount = 2;

}

PeakLimiter::PeakLimiter(std::uint32_t sampleRateHz, float inputGain)
    : release_(1.0F - std::exp(-1.0F / (kReleaseSeconds * static_cast<float>(sampleRateHz)))),
      inputGain_(inputGain) {}

void PeakLimiter::Process(std::span<float> interleavedStereo) noexcept {
    for (std::size_t frame = 0; frame + 1 < interleavedStereo.size(); frame += kChannelCount) {
        float& left = interleavedStereo[frame];
        float& right = interleavedStereo[frame + 1];
        left *= inputGain_;
        right *= inputGain_;
        const float peak = std::max(std::abs(left), std::abs(right));
        if (peak * gain_ > 1.0F) {
            gain_ = 1.0F / peak;
        }
        left *= gain_;
        right *= gain_;
        gain_ += (1.0F - gain_) * release_;
    }
}

}
