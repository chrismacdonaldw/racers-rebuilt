#include "audio/peak_limiter.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace rr::audio {
namespace {

constexpr std::uint32_t kRateHz = 22050;

std::vector<float> Stereo(std::size_t frameCount, float level) {
    return std::vector<float>(frameCount * 2, level);
}

float PeakOf(const std::vector<float>& samples) {
    return std::ranges::max(samples, {}, [](float sample) { return std::abs(sample); });
}

TEST_CASE("a mix within full scale passes unchanged") {
    PeakLimiter limiter(kRateHz, 1.0F);
    std::vector<float> mix = Stereo(256, -0.9F);

    limiter.Process(mix);

    CHECK(mix == Stereo(256, -0.9F));
}

TEST_CASE("a mix past full scale is held to it") {
    PeakLimiter limiter(kRateHz, 1.0F);
    std::vector<float> mix = Stereo(256, 4.0F);
    mix[101] = -6.0F;

    limiter.Process(mix);

    CHECK(PeakOf(mix) <= 1.0F);
    CHECK(std::abs(mix[101]) == doctest::Approx(1.0F));
}

TEST_CASE("the input gain scales the mix before the limit") {
    PeakLimiter limiter(kRateHz, 0.5F);
    std::vector<float> mix = Stereo(256, 1.6F);

    limiter.Process(mix);

    CHECK(mix == Stereo(256, 0.8F));
}

TEST_CASE("the gain eases back to unity after a peak") {
    PeakLimiter limiter(kRateHz, 1.0F);
    std::vector<float> peak = Stereo(1, 2.0F);
    limiter.Process(peak);
    std::vector<float> quiet = Stereo(kRateHz, 0.1F);

    limiter.Process(quiet);

    CHECK(quiet.front() == doctest::Approx(0.05F).epsilon(0.01));
    CHECK(limiter.Gain() == doctest::Approx(1.0F).epsilon(0.01));
}

}
}
