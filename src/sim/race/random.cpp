#include "sim/race/random.hpp"

#include "core/assert.hpp"

namespace rr::sim {
namespace {

constexpr std::uint64_t kMultiplier = 6364136223846793005ULL;
constexpr std::uint64_t kIncrement = 1442695040888963407ULL;
constexpr unsigned kXorShift = 18U;
constexpr unsigned kOutputShift = 27U;
constexpr unsigned kRotateShift = 59U;
constexpr unsigned kRotateMask = 31U;

}

Random::Random(std::uint64_t seed) {
    static_cast<void>(Next());
    state_ += seed;
    static_cast<void>(Next());
}

std::uint32_t Random::Next() {
    const std::uint64_t old = state_;
    state_ = (old * kMultiplier) + kIncrement;
    const auto mixed = static_cast<std::uint32_t>(((old >> kXorShift) ^ old) >> kOutputShift);
    const auto rotation = static_cast<unsigned>(old >> kRotateShift);
    return (mixed >> rotation) | (mixed << ((0U - rotation) & kRotateMask));
}

std::uint32_t Random::Below(std::uint32_t bound) {
    RR_ASSERT(bound > 0);
    const std::uint32_t threshold = (0U - bound) % bound;
    while (true) {
        const std::uint32_t value = Next();
        if (value >= threshold) {
            return value % bound;
        }
    }
}

}
