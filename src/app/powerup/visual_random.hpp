#pragma once

#include <cstdint>

namespace rr::app {

class VisualRandom {
public:
    explicit VisualRandom(std::uint32_t seed) : state_(seed == 0 ? 1U : seed) {}

    /// Uniform in [0, 1).
    [[nodiscard]] float Next() {
        state_ ^= state_ << 13U;
        state_ ^= state_ >> 17U;
        state_ ^= state_ << 5U;
        return static_cast<float>(state_ >> kDroppedBits) * kScale;
    }

private:
    static constexpr std::uint32_t kDroppedBits = 8;
    static constexpr float kScale = 1.0F / 16'777'216.0F;

    std::uint32_t state_;
};

}
