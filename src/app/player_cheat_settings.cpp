#include "app/player_cheat_settings.hpp"

#include <chrono>

namespace rr::app {

void DeclarePlayerCheatSettings(settings::Registry& registry) {
    registry.Declare(kInvincibleCheat);
    registry.Declare(kBottomlessBrickCheat);
    registry.Declare(kFullWhiteCheat);
    registry.Declare(kBrickMagnetCheat);
    registry.Declare(kLongTurboCheat);
    registry.Declare(kPerfectStartCheat);
    registry.Declare(kFreeWarpCheat);
    registry.Declare(kHeadStartCheat);
    registry.Declare(kBoostButtonCheat);
    registry.Declare(kSlowMotionCheat);
    registry.Declare(kSkipAheadCheat);
}

sim::PlayerCheats PlayerCheatsFrom(const settings::Registry& registry) {
    return {.isInvincible = registry.Get(kInvincibleCheat),
            .hasBottomlessBrick = registry.Get(kBottomlessBrickCheat),
            .keepsFullWhite = registry.Get(kFullWhiteCheat),
            .hasBrickMagnet = registry.Get(kBrickMagnetCheat),
            .hasLongTurbo = registry.Get(kLongTurboCheat),
            .hasPerfectStart = registry.Get(kPerfectStartCheat),
            .hasFreeWarp = registry.Get(kFreeWarpCheat),
            .hasBoostButton = registry.Get(kBoostButtonCheat),
            .hasSkipAhead = registry.Get(kSkipAheadCheat),
            .headStart = std::chrono::duration_cast<Milliseconds>(
                std::chrono::seconds(registry.Get(kHeadStartCheat)))};
}

}
