#include "sim/ai/powerup_habits.hpp"

#include <algorithm>

namespace rr::sim {
namespace {

constexpr float kBaseChance = 0.2F;
constexpr float kStatChance = 0.8F;
constexpr float kFullStat = 90.0F;
constexpr float kByteScale = 255.0F;
constexpr std::int32_t kChargeColourCodes = 4;

std::optional<assets::BrickColour> ChargeColour(std::int32_t code) {
    if (code < 1 || code > kChargeColourCodes) {
        return std::nullopt;
    }
    return static_cast<assets::BrickColour>(code - 1);
}

}

std::uint8_t UseChance(std::int32_t stat) {
    const float chance =
        std::min(1.0F, kBaseChance + (static_cast<float>(stat) * kStatChance / kFullStat));
    return static_cast<std::uint8_t>(chance * kByteScale);
}

PowerupHabits HabitsOf(const assets::DriverEntry& driver) {
    PowerupHabits habits;
    const auto chance = [&](assets::BrickColour colour, std::int32_t stat) {
        const auto index = static_cast<std::size_t>(colour);
        habits.useChances[index] = driver.useChances[index].value_or(UseChance(stat));
    };
    chance(assets::BrickColour::Red, driver.redStat);
    chance(assets::BrickColour::Blue, driver.blueStat);
    chance(assets::BrickColour::Green, driver.greenStat);
    chance(assets::BrickColour::Yellow, driver.yellowStat);
    habits.startBoost = driver.startBoost;
    habits.chargeColour = ChargeColour(driver.aiChargeColor);
    habits.chargeWhites =
        habits.chargeColour ? static_cast<std::size_t>(std::max(0, driver.aiWhiteBrickTarget)) : 0;
    return habits;
}

}
