#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "core/duration.hpp"
#include "sim/ai/powerup_habits.hpp"
#include "sim/physics/racer.hpp"
#include "sim/powerup/powerup_inventory.hpp"
#include "sim/race/random.hpp"

namespace rr::sim {

inline constexpr Milliseconds kAiUseCheck{150};
inline constexpr Milliseconds kAiUseHold{1000};

struct AiPowerupView {
    std::span<const Racer> racers;
    std::size_t racer = 0;
    const PowerupInventory* inventory = nullptr;
    bool isCrowded = false;
    bool hasFinished = false;
};

class AiPowerupUser {
public:
    explicit AiPowerupUser(const PowerupHabits& habits) : habits_(habits) {}

    [[nodiscard]] bool Tick(Milliseconds elapsed, const AiPowerupView& view, Random& random);

    [[nodiscard]] bool RollsStartBoost(Random& random) const;

private:
    enum class Choice : std::uint8_t { Use, Recheck, Hold };

    [[nodiscard]] Choice Choose(const AiPowerupView& view, Random& random) const;
    [[nodiscard]] bool IsCharging(const PowerupInventory& inventory) const;

    PowerupHabits habits_;
    Milliseconds sinceCheck_{0};
    Milliseconds wait_ = kAiUseCheck;
};

}
