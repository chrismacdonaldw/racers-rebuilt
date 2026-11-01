#include "app/gameplay_fingerprint.hpp"

#include <format>

#include "mods/mod_manifest.hpp"

namespace rr::app {

std::vector<std::string> GameplayFingerprint(const settings::Registry& settings,
                                             const mods::ModList& mods) {
    std::vector<std::string> entries;
    for (const mods::Mod* mod : mods.Enabled(settings)) {
        entries.push_back(std::format("mod:{}@{}", mod->Manifest().id, mod->Manifest().version));
    }
    return entries;
}

}
