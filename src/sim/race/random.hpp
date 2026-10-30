#pragma once

#include <cstdint>

namespace rr::sim {

/// PCG32, the only source of randomness in a race, so its seed replays it.
class Random {
public:
    explicit Random(std::uint64_t seed);

    [[nodiscard]] std::uint32_t Next();

    /// Uniform in `[0, bound)`. `bound` must be above 0.
    [[nodiscard]] std::uint32_t Below(std::uint32_t bound);

private:
    std::uint64_t state_ = 0;
};

}
