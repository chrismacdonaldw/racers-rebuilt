#include "app/race_scene_settings.hpp"

#include "app/cheat_settings.hpp"
#include "app/fix_settings.hpp"
#include "app/game_settings.hpp"
#include "app/modifier_settings.hpp"
#include "app/player_cheat_settings.hpp"
#include "app/warp_settings.hpp"

namespace rr::app {

void ApplyUnnamedSettings(RaceSceneSetup& setup, const settings::Registry& settings,
                          const sim::Cheats& session) {
    if (!setup.isDemo && !setup.isTestDrive) {
        setup.modifiers = ModifiersFrom(settings);
        setup.playerCheats = PlayerCheatsFrom(settings);
    }
    setup.isMirrored = LoadsMirrored(setup);
    setup.cheats = RaceCheats(setup.cheats, session);
    setup.carBody = CarBodyFrom(settings);
    setup.version = SelectedVersion(settings);
    setup.lagsInput = settings.Get(kInputLag2001);
    setup.warp = WarpRulesFrom(settings);
}

}
