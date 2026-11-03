#include "app/gameplay_fingerprint.hpp"

#include <format>

#include "app/racer/racer_ref.hpp"
#include "mods/mod_manifest.hpp"

namespace rr::app {

std::vector<std::string> GameplayFingerprint(const settings::Registry& settings,
                                             const mods::ModList& mods,
                                             const std::optional<settings::Racer>& player) {
    std::vector<std::string> entries;
    for (const mods::Mod* mod : mods.Enabled(settings)) {
        entries.push_back(std::format("mod:{}@{}", mod->Manifest().id, mod->Manifest().version));
    }
    if (player) {
        entries.push_back(std::format("racer:{}@{:016x}", player->name, RacerHash(*player)));
    }
    return entries;
}

}
