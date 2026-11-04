#include "app/modifier_settings.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace rr::app {
namespace {

constexpr float kPercent = 100.0F;

std::optional<sim::PowerupKind> OnlyPowerup(std::string_view choice) {
    const auto found = std::ranges::find(kOnlyPowerupChoices, choice);
    if (found == kOnlyPowerupChoices.begin() || found == kOnlyPowerupChoices.end()) {
        return std::nullopt;
    }
    return static_cast<sim::PowerupKind>(found - kOnlyPowerupChoices.begin() - 1);
}

}

void DeclareModifierSettings(settings::Registry& registry) {
    registry.Declare(kGravityModifier);
    registry.Declare(kSpeedModifier);
    registry.Declare(kMirrorModifier);
    registry.Declare(kOnlyPowerupModifier);
    registry.Declare(kChaosBricksModifier);
    registry.Declare(kSqueakyVoicesModifier);
    registry.Declare(kRocketRushModifier);
}

RaceModifiers ModifiersFrom(const settings::Registry& registry) {
    const std::string onlyPowerup = registry.Get(kOnlyPowerupModifier);
    return {.rules = {.gravityScale = static_cast<float>(registry.Get(kGravityModifier)) / kPercent,
                      .speedScale = static_cast<float>(registry.Get(kSpeedModifier)) / kPercent,
                      .onlyPowerup = OnlyPowerup(onlyPowerup),
                      .hasChaosBricks = registry.Get(kChaosBricksModifier)},
            .mirrorsTracks = registry.Get(kMirrorModifier),
            .squeaksVoices = registry.Get(kSqueakyVoicesModifier),
            .rushesRocketRacer = registry.Get(kRocketRushModifier)};
}

}
