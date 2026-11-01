#include "audio/volume_curve.hpp"

#include <algorithm>
#include <cmath>

namespace rr::audio {
namespace {

constexpr float kVolumeRangeDb = 30.0F;
constexpr float kSilentVolume = 0.005F;
constexpr float kPanRangeDb = 25.0F;

float GainOfDb(float decibels) {
    return std::pow(10.0F, decibels / 20.0F);
}

}

float VolumeGain(float volume) {
    if (volume < kSilentVolume) {
        return 0.0F;
    }
    return GainOfDb((std::min(volume, 1.0F) - 1.0F) * kVolumeRangeDb);
}

float PanBalance(float pan) {
    if (std::abs(pan) >= 1.0F) {
        return std::copysign(1.0F, pan);
    }
    return std::copysign(1.0F - GainOfDb(-std::abs(pan) * kPanRangeDb), pan);
}

}
