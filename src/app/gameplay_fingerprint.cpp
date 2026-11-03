#include "app/gameplay_fingerprint.hpp"

#include <format>

#include "app/racer/racer_ref.hpp"
#include "mods/mod_manifest.hpp"

namespace rr::app {

std::vector<std::string> GameplayFingerprint(const settings::Registry& settings,
                                             const mods::ModList& mods,
                                             std::optional<std::uint64_t> overridesHash,
                                             const PlayerRacers& players) {
    std::vector<std::string> entries;
    for (const mods::Mod* mod : mods.Enabled(settings)) {
        entries.push_back(std::format("mod:{}@{}", mod->Manifest().id, mod->Manifest().version));
    }
    if (overridesHash) {
        entries.push_back(std::format("overrides:{:016x}", *overridesHash));
    }
    for (const auto& [key, racer] :
         {std::pair{"racer", &players.first}, std::pair{"racer2", &players.second}}) {
        if (*racer) {
            entries.push_back(
                std::format("{}:{}@{:016x}", key, (*racer)->name, RacerHash(**racer)));
        }
    }
    return entries;
}

}
