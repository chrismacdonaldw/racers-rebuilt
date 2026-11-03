#include "app/cheat_settings.hpp"

namespace rr::app {

void DeclareCheatSettings(settings::Registry& registry) {
    for (const CheatToggle& toggle : kCheatToggles) {
        registry.Declare(toggle.setting);
    }
}

sim::Cheats RaceCheats(const sim::Cheats& named, const sim::Cheats& session) {
    return named.IsEmpty() ? session : named;
}

sim::Cheats WithCheatToggles(sim::Cheats session, const settings::Registry& registry) {
    for (const CheatToggle& toggle : kCheatToggles) {
        if (registry.Get(toggle.setting) && !session.Has(toggle.cheat)) {
            session.EnterPlate(sim::PlateOf(toggle.cheat));
        }
    }
    return session;
}

}
